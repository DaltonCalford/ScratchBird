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
 * PostgreSQL Wire Protocol Adapter Implementation
 *
 * PostgreSQL-compatible front-door implementation.
 */

#include "scratchbird/protocol/adapters/postgresql_adapter.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/parser/v3_compiler.h"
#include "scratchbird/security/parser_auth_policy.h"
#include "scratchbird/sblr/postgresql_query_compiler.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/client/connection.h"

#include <filesystem>
#include <array>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <random>
#include <sstream>
#include <iomanip>
#include <unordered_set>

// PostgreSQL wire compatibility still requires the legacy MD5 exchange for
// older clients/roles. This code is retained for interoperability, not as
// guidance for new password policy design.
// For MD5
#ifdef HAVE_OPENSSL
#include <openssl/md5.h>
#else
// Simple MD5 fallback (for testing only)
#include <functional>
#endif

namespace scratchbird {
namespace protocol {

std::mutex PostgresqlAdapter::backend_registry_mutex_;
std::unordered_map<int32_t, PostgresqlAdapter::BackendEntry> PostgresqlAdapter::backend_registry_;

namespace {

std::string buildPostgresqlSchemaPath(const std::string& db_name) {
    return "emulated.postgresql.localhost.databases." + db_name;
}

std::string buildLegacyPostgresqlSchemaPath(const std::string& db_name) {
    return "remote.emulation.postgresql.localhost.databases." + db_name;
}

std::string extractEmulatedDatabaseName(const std::string& selected_database) {
    if (selected_database.empty()) {
        return selected_database;
    }

    std::string normalized = selected_database;
    std::replace(normalized.begin(), normalized.end(), '/', '.');
    while (!normalized.empty() && normalized.front() == '.') {
        normalized.erase(normalized.begin());
    }
    while (!normalized.empty() && normalized.back() == '.') {
        normalized.pop_back();
    }
    if (normalized.empty()) {
        return selected_database;
    }

    std::string lower = normalized;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    constexpr const char* marker = ".databases.";
    size_t marker_pos = lower.rfind(marker);
    if (marker_pos != std::string::npos) {
        std::string leaf = normalized.substr(marker_pos + std::strlen(marker));
        if (!leaf.empty()) {
            return leaf;
        }
    }

    static const std::array<std::string, 4> emulated_prefixes = {
        "emulated.postgresql.",
        "emulation.postgresql.",
        "remote.emulation.postgresql.",
        "remote.emulated.postgresql.",
    };
    for (const auto& prefix : emulated_prefixes) {
        if (lower.rfind(prefix, 0) == 0) {
            size_t last_dot = normalized.rfind('.');
            if (last_dot != std::string::npos && (last_dot + 1) < normalized.size()) {
                return normalized.substr(last_dot + 1);
            }
            break;
        }
    }

    return normalized;
}

std::string resolveEmulatedDatabaseForSession(
    const std::unordered_map<std::string, std::string>& client_parameters,
    const std::string& fallback_database) {
    auto it = client_parameters.find("database");
    if (it != client_parameters.end()) {
        std::string emulated_name = extractEmulatedDatabaseName(it->second);
        if (!emulated_name.empty()) {
            return emulated_name;
        }
    }
    return extractEmulatedDatabaseName(fallback_database);
}

std::string resolvePostgresqlSchemaPath(core::CatalogManager* catalog,
                                        const std::string& db_name) {
    std::string db_key = extractEmulatedDatabaseName(db_name);
    if (db_key.empty()) {
        db_key = db_name;
    }
    std::string canonical = buildPostgresqlSchemaPath(db_key);
    if (!catalog) {
        return canonical;
    }

    core::CatalogManager::SchemaInfo schema_info;
    core::ErrorContext check_ctx;
    if (catalog->getSchema(canonical, schema_info, &check_ctx) == core::Status::OK) {
        return canonical;
    }
    // Always use canonical emulated path; legacy aliases are not used as session roots.
    return canonical;
}

std::string ensurePostgresqlSchemaPath(core::CatalogManager* catalog,
                                       const std::string& db_name,
                                       core::ErrorContext* ctx) {
    std::string selected = resolvePostgresqlSchemaPath(catalog, db_name);
    if (!catalog || selected.empty()) {
        return selected;
    }

    core::CatalogManager::SchemaInfo schema_info;
    core::ErrorContext check_ctx;
    if (catalog->getSchema(selected, schema_info, &check_ctx) == core::Status::OK) {
        return selected;
    }

    core::ID schema_id;
    core::ErrorContext create_ctx;
    auto create_status = catalog->createSchemaPath(selected,
                                                   core::CatalogManager::SchemaType::REMOTE_EMULATED,
                                                   schema_id,
                                                   &create_ctx);
    if (create_status != core::Status::OK &&
        create_status != core::Status::FILE_EXISTS) {
        if (ctx && ctx->message.empty() && !create_ctx.message.empty()) {
            ctx->set(create_status, create_ctx.message.c_str(), __FILE__, __LINE__, __func__);
        }
    }
    return selected;
}

std::string trimAscii(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string toUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

bool isPostgresqlSystemDatabaseName(const std::string& database_name) {
    const std::string normalized = toUpperAscii(extractEmulatedDatabaseName(database_name));
    return normalized == "POSTGRES" ||
           normalized == "TEMPLATE0" ||
           normalized == "TEMPLATE1";
}

std::string normalizePostgresqlScramClientFirst(const std::string& client_first,
                                                const std::string& username) {
    // Preserve the exact client-first-message-bare sent by libpq/psql.
    // Rewriting "n=," to embed startup username breaks the SCRAM proof.
    static_cast<void>(username);
    return client_first;
}

bool parseInheritsParentNames(const std::string& sql, std::vector<std::string>& parents_out) {
    parents_out.clear();
    std::string trimmed = trimAscii(sql);
    if (trimmed.empty()) {
        return false;
    }

    std::string upper = toUpperAscii(trimmed);
    if (upper.rfind("CREATE", 0) != 0 || upper.find("TABLE") == std::string::npos) {
        return false;
    }

    const size_t inherits_pos = upper.find("INHERITS");
    if (inherits_pos == std::string::npos) {
        return false;
    }

    const size_t open = trimmed.find('(', inherits_pos);
    if (open == std::string::npos) {
        return false;
    }

    bool in_quotes = false;
    int depth = 0;
    size_t close = std::string::npos;
    for (size_t i = open; i < trimmed.size(); ++i) {
        char ch = trimmed[i];
        if (ch == '"') {
            in_quotes = !in_quotes;
        }
        if (in_quotes) {
            continue;
        }
        if (ch == '(') {
            ++depth;
        } else if (ch == ')') {
            --depth;
            if (depth == 0) {
                close = i;
                break;
            }
        }
    }
    if (close == std::string::npos || close <= open + 1) {
        return false;
    }

    std::string list = trimmed.substr(open + 1, close - open - 1);
    std::string token;
    in_quotes = false;
    for (char ch : list) {
        if (ch == '"') {
            in_quotes = !in_quotes;
            token.push_back(ch);
            continue;
        }
        if (ch == ',' && !in_quotes) {
            std::string parent = trimAscii(token);
            if (!parent.empty()) {
                std::string parent_upper = toUpperAscii(parent);
                if (parent_upper.rfind("ONLY ", 0) == 0) {
                    parent = trimAscii(parent.substr(5));
                }
                parents_out.push_back(parent);
            }
            token.clear();
            continue;
        }
        token.push_back(ch);
    }

    std::string tail = trimAscii(token);
    if (!tail.empty()) {
        std::string tail_upper = toUpperAscii(tail);
        if (tail_upper.rfind("ONLY ", 0) == 0) {
            tail = trimAscii(tail.substr(5));
        }
        parents_out.push_back(tail);
    }

    return !parents_out.empty();
}

void collectInheritsMergeNotices(client::Connection* client_conn,
                                 const std::string& sql,
                                 std::vector<std::string>& notices_out) {
    if (client_conn == nullptr) {
        return;
    }

    std::vector<std::string> parents;
    if (!parseInheritsParentNames(sql, parents) || parents.size() < 2) {
        return;
    }

    struct ColumnSignature {
        protocol::WireType type = protocol::WireType::UNKNOWN;
        int32_t type_modifier = 0;
        std::string display_name;
    };

    std::unordered_map<std::string, ColumnSignature> seen_columns;
    std::unordered_set<std::string> emitted;

    for (const auto& parent : parents) {
        client::ResultSet parent_rs;
        core::ErrorContext parent_ctx;
        const std::string probe_sql = "SELECT * FROM " + parent + " LIMIT 0";
        if (client_conn->executeQuery(probe_sql, &parent_rs, &parent_ctx) != core::Status::OK) {
            continue;
        }

        for (const auto& column : parent_rs.getColumns()) {
            const std::string key = toLowerAscii(column.name);
            auto it = seen_columns.find(key);
            if (it == seen_columns.end()) {
                ColumnSignature sig;
                sig.type = column.type;
                sig.type_modifier = column.type_modifier;
                sig.display_name = column.name;
                seen_columns.emplace(key, std::move(sig));
                continue;
            }

            if (it->second.type == column.type &&
                it->second.type_modifier == column.type_modifier &&
                emitted.insert(key).second) {
                const std::string& display_name =
                    it->second.display_name.empty() ? column.name : it->second.display_name;
                notices_out.push_back(
                    "merging multiple inherited definitions of column \"" +
                    display_name + "\"");
            }
        }
    }
}

bool tryBuildShowServerParameterResult(
    const std::string& query,
    const std::unordered_map<std::string, std::string>& server_parameters,
    ResultContext& result) {
    std::string normalized = trimAscii(query);
    if (!normalized.empty() && normalized.back() == ';') {
        normalized.pop_back();
        normalized = trimAscii(normalized);
    }

    if (normalized.size() < 5) {
        return false;
    }
    std::string upper = normalized;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    if (upper.rfind("SHOW ", 0) != 0) {
        return false;
    }

    std::string requested_name = trimAscii(normalized.substr(5));
    if (requested_name.empty()) {
        return false;
    }

    if ((requested_name.front() == '"' && requested_name.back() == '"') ||
        (requested_name.front() == '\'' && requested_name.back() == '\'')) {
        requested_name = requested_name.substr(1, requested_name.size() - 2);
    }

    const std::string requested_key = toLowerAscii(requested_name);
    std::string value;
    bool found = false;
    for (const auto& entry : server_parameters) {
        if (toLowerAscii(entry.first) == requested_key) {
            value = entry.second;
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }

    result.columns.clear();
    ProtocolCodec::ColumnInfo column;
    column.name = requested_key;
    column.type = WireType::VARCHAR;
    result.columns.push_back(column);

    result.rows.clear();
    std::vector<ProtocolCodec::ColumnValue> row;
    row.push_back(ProtocolCodec::ColumnValue::fromString(value));
    result.rows.push_back(std::move(row));

    result.rows_affected = 1;
    result.command_tag = "SHOW";
    result.has_error = false;
    return true;
}

bool pgWireDebugEnabled() {
    static const bool enabled = []() {
        const char* value = std::getenv("SCRATCHBIRD_PG_DEBUG_WIRE");
        if (!value || value[0] == '\0') {
            return false;
        }
        std::string normalized(value);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        return normalized != "0" &&
               normalized != "FALSE" &&
               normalized != "NO" &&
               normalized != "OFF";
    }();
    return enabled;
}

} // namespace

// ============================================================================
// Constructor/Destructor
// ============================================================================

PostgresqlAdapter::PostgresqlAdapter(const ProtocolAdapterConfig& config)
    : ProtocolAdapter(config) {
    // Initialize default server parameters
    initializeServerParameters();

    // Generate backend key data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int32_t> dist;
    backend_pid_ = dist(gen) & 0x7FFFFFFF;  // Positive value
    backend_secret_key_ = dist(gen);

    // Generate MD5 salt
    for (int i = 0; i < 4; ++i) {
        md5_salt_[i] = static_cast<uint8_t>(dist(gen) & 0xFF);
    }

    registerBackend();
}

PostgresqlAdapter::~PostgresqlAdapter() {
    unregisterBackend();
}

void PostgresqlAdapter::registerBackend() {
    std::lock_guard<std::mutex> lock(backend_registry_mutex_);
    backend_registry_[backend_pid_] = BackendEntry{this, backend_secret_key_};
}

void PostgresqlAdapter::unregisterBackend() {
    std::lock_guard<std::mutex> lock(backend_registry_mutex_);
    auto it = backend_registry_.find(backend_pid_);
    if (it != backend_registry_.end() && it->second.adapter == this) {
        backend_registry_.erase(it);
    }
}

PostgresqlAdapter* PostgresqlAdapter::findBackend(int32_t pid, int32_t key) {
    std::lock_guard<std::mutex> lock(backend_registry_mutex_);
    auto it = backend_registry_.find(pid);
    if (it == backend_registry_.end()) {
        return nullptr;
    }
    if (it->second.secret_key != key) {
        return nullptr;
    }
    return it->second.adapter;
}

void PostgresqlAdapter::requestCancel() {
    cancel_requested_.store(true, std::memory_order_release);
    if (client_) {
        client_->disconnect();
        client_.reset();
        search_path_set_ = false;
    }
}
core::Status PostgresqlAdapter::connectRemoteClient(core::ErrorContext* ctx) {
    if (client_) {
        return core::Status::OK;
    }

    std::string selected_database;
    if (!resolveDatabaseSelection(database_name_, selected_database)) {
        if (config_.enforce_bound_database &&
            !config_.default_database.empty() &&
            isPostgresqlSystemDatabaseName(database_name_)) {
            selected_database = config_.default_database;
        } else {
            if (ctx) {
                ctx->set(core::Status::INVALID_AUTHORIZATION,
                         "Database switch denied by manager binding context",
                         __FILE__, __LINE__, __func__);
            }
            return core::Status::INVALID_AUTHORIZATION;
        }
    }
    database_name_ = selected_database;
    client_config_.database_name = selected_database;
    if (!config_.engine_endpoint.empty()) {
        // Listener supplies a concrete IPC socket path; avoid AUTO fallback.
        client_config_.ipc_method = server::IPCMethod::UNIX_SOCKET;
        client_config_.socket_path = config_.engine_endpoint;
    } else {
        client_config_.ipc_method = server::IPCMethod::UNIX_SOCKET;
        client_config_.socket_path = server::getIPCPath(client_config_.database_name,
                                                        client_config_.ipc_method);
    }
    client_config_.connect_timeout_ms = config_.read_timeout_ms;
    client_config_.read_timeout_ms = config_.read_timeout_ms;
    client_config_.write_timeout_ms = config_.write_timeout_ms;
    client_config_.auto_commit = true;
    client_config_.auto_start_server = false;
    client_config_.connect_client_flags = config_.connect_client_flags;
    client_config_.has_bound_db_uuid = config_.has_bound_db_uuid;
    client_config_.bound_db_uuid = config_.bound_db_uuid;
    client_config_.username = username_.empty() ? "BOOTSTRAP" : username_;

    client_ = std::make_unique<client::Connection>();
    auto status = client_->connect(client_config_, ctx);
    if (status != core::Status::OK) {
        client_.reset();
        return status;
    }

    return core::Status::OK;
}

core::Status PostgresqlAdapter::ensureRemoteClient(core::ErrorContext* ctx) {
    auto status = connectRemoteClient(ctx);
    if (status != core::Status::OK) {
        return status;
    }

    core::ErrorContext catalog_ctx;
    core::Status catalog_status = ensurePostgresSystemCatalog(&catalog_ctx);
    if (catalog_status != core::Status::OK && pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] ensureRemoteClient catalog bootstrap status=%d err=%s\n",
                     static_cast<int>(catalog_status),
                     catalog_ctx.message.c_str());
        std::fflush(stderr);
    }

    // Set search_path to the selected emulated DB schema root.
    if (!search_path_set_) {
        std::string db_name = resolveEmulatedDatabaseForSession(client_parameters_, database_name_);
        if (db_name.empty()) {
            db_name = database_name_;
        }
        auto* catalog = engineDatabase() ? engineDatabase()->catalog_manager() : nullptr;
        std::string schema_root = ensurePostgresqlSchemaPath(catalog, db_name, ctx);
        if (pgWireDebugEnabled()) {
            std::fprintf(stderr,
                         "[pg_wire] ensureRemoteClient db_name=%s schema_root=%s bound_db=%s\n",
                         db_name.c_str(),
                         schema_root.c_str(),
                         database_name_.c_str());
            std::fflush(stderr);
        }
        auto execute_sql = [&](const std::string& sql_text) -> core::Status {
            client::ResultSet rs;
            parser::v3::Compiler compiler;
            auto compile_result = compiler.compile(sql_text);
            if (!compile_result.ok) {
                return core::Status::INVALID_ARGUMENT;
            }
            return client_->executeBytecode(compile_result.bytecode, sql_text, &rs, ctx);
        };
        auto execute_set = [&](const std::string& target) -> core::Status {
            std::string set_path = "SET search_path TO '" + escapeLiteral(target) + "'";
            return execute_sql(set_path);
        };
        auto execute_set_with_pg_catalog = [&](const std::string& target) -> core::Status {
            std::string set_path = "SET search_path TO pg_catalog, '" + escapeLiteral(target) + "'";
            return execute_sql(set_path);
        };
        auto execute_create_database = [&](const std::string& logical_db_name) -> core::Status {
            std::string ddl = "CREATE DATABASE IF NOT EXISTS \"" + logical_db_name + "\"";
            client::ResultSet rs;
            sblr::PostgreSQLQueryCompiler compiler(engineDatabase());
            compiler.setDefaultSchema(schema_root);
            auto compile_result = compiler.compile(ddl);
            if (!compile_result.success()) {
                if (ctx && ctx->message.empty()) {
                    const auto& errors = compile_result.errors();
                    const std::string message = errors.empty()
                        ? "Failed to compile PostgreSQL emulation database bootstrap DDL"
                        : errors.front();
                    ctx->set(core::Status::INVALID_ARGUMENT,
                             message.c_str(),
                             __FILE__, __LINE__, __func__);
                }
                return core::Status::INVALID_ARGUMENT;
            }
            return client_->executeBytecode(compile_result.bytecode(),
                                            ddl,
                                            &rs,
                                            ctx);
        };
        auto ensure_root_and_set = [&](const std::string& target) -> core::Status {
            core::Status status_local = execute_set_with_pg_catalog(target);
            if (status_local != core::Status::OK) {
                // Compatibility fallback if parser/runtime rejects multi-entry form.
                status_local = execute_set(target);
            }
            if (status_local == core::Status::OK) {
                return status_local;
            }

            core::Status create_status = execute_create_database(db_name);
            if (catalog) {
                core::ID schema_id;
                core::ErrorContext create_ctx;
                core::Status schema_status = catalog->createSchemaPath(
                    target,
                    core::CatalogManager::SchemaType::REMOTE_EMULATED,
                    schema_id,
                    &create_ctx);
                if (create_status == core::Status::OK &&
                    schema_status != core::Status::OK &&
                    schema_status != core::Status::FILE_EXISTS) {
                    create_status = schema_status;
                }
            }

            if (create_status != core::Status::OK &&
                create_status != core::Status::FILE_EXISTS) {
                return status_local;
            }
            core::Status retry_status = execute_set_with_pg_catalog(target);
            if (retry_status != core::Status::OK) {
                retry_status = execute_set(target);
            }
            return retry_status;
        };

        auto set_status = ensure_root_and_set(schema_root);
        if (pgWireDebugEnabled()) {
            std::string client_err = client_ ? client_->getLastError() : std::string();
            std::fprintf(stderr,
                         "[pg_wire] ensureRemoteClient set_search_path target=%s status=%d err=%s\n",
                         schema_root.c_str(),
                         static_cast<int>(set_status),
                         client_err.c_str());
            std::fflush(stderr);
        }
        if (set_status != core::Status::OK) {
            const std::string fallback_schema = "users.public";
            core::Status fallback_status = execute_set_with_pg_catalog(fallback_schema);
            if (fallback_status != core::Status::OK) {
                fallback_status = execute_set(fallback_schema);
            }
            if (fallback_status == core::Status::OK) {
                server_parameters_["search_path"] = "pg_catalog, " + fallback_schema;
                search_path_set_ = true;
                return core::Status::OK;
            }

            std::string client_err = client_ ? client_->getLastError() : std::string();
            if (ctx && ctx->message.empty() && !client_err.empty()) {
                ctx->set(set_status,
                         client_err.c_str(),
                         __FILE__, __LINE__, __func__);
            }
            if (ctx && ctx->message.empty()) {
                ctx->set(set_status,
                         "Failed to set PostgreSQL emulation root schema",
                         __FILE__, __LINE__, __func__);
            }
            return set_status;
        }

        server_parameters_["search_path"] = "pg_catalog, " + schema_root;
        search_path_set_ = true;
    }

    return core::Status::OK;
}

core::Status PostgresqlAdapter::executeRemoteQuery(const QueryContext& query,
                                                   ResultContext& result,
                                                   core::ErrorContext* ctx) {
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] executeRemoteQuery begin sql=%s\n",
                     query.query.c_str());
        std::fflush(stderr);
    }

    auto status = ensureRemoteClient(ctx);
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        result.error_message = ctx ? ctx->message : "Failed to connect to engine";
        if (pgWireDebugEnabled()) {
            std::fprintf(stderr,
                         "[pg_wire] executeRemoteQuery ensureRemoteClient status=%d err=%s\n",
                         static_cast<int>(status),
                         result.error_message.c_str());
            std::fflush(stderr);
        }
        return status;
    }

    if (cancel_requested_.load(std::memory_order_acquire)) {
        cancel_requested_.store(false, std::memory_order_release);
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(core::Status::QUERY_CANCELED);
        result.error_message = "Query canceled";
        return core::Status::QUERY_CANCELED;
    }

    client::ResultSet rs;
    QueryContext rewritten = query;
    if (!query.parameter_values.empty()) {
        rewritten.query = substitutePositionalParameters(query);
    }

    std::vector<uint8_t> bytecode;
    std::string compile_error;
    auto compile_status = compileQuery(rewritten.query, bytecode, compile_error);
    if (compile_status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(compile_status);
        result.error_message = compile_error.empty() ? "Compilation failed" : compile_error;
        if (pgWireDebugEnabled()) {
            std::fprintf(stderr,
                         "[pg_wire] executeRemoteQuery compile status=%d err=%s\n",
                         static_cast<int>(compile_status),
                         result.error_message.c_str());
            std::fflush(stderr);
        }
        return compile_status;
    }
    if (pgWireDebugEnabled()) {
        uint64_t digest = 1469598103934665603ULL;
        std::ostringstream head_hex;
        head_hex << std::hex << std::setfill('0');
        const size_t preview = std::min<size_t>(bytecode.size(), 24);
        for (size_t i = 0; i < bytecode.size(); ++i) {
            digest ^= static_cast<uint64_t>(bytecode[i]);
            digest *= 1099511628211ULL;
            if (i < preview) {
                head_hex << std::setw(2)
                         << static_cast<unsigned>(bytecode[i]);
            }
        }
        std::fprintf(stderr,
                     "[pg_wire] executeRemoteQuery compile ok bytecode=%zu digest=%016llx head=%s\n",
                     bytecode.size(),
                     static_cast<unsigned long long>(digest),
                     head_hex.str().c_str());
        std::fflush(stderr);
    }

    status = client_->executeBytecode(bytecode, query.query, &rs, ctx);
    if (cancel_requested_.load(std::memory_order_acquire)) {
        cancel_requested_.store(false, std::memory_order_release);
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(core::Status::QUERY_CANCELED);
        result.error_message = "Query canceled";
        return core::Status::QUERY_CANCELED;
    }
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        std::string err = client_->getLastError();
        if (err.empty() && ctx) {
            err = ctx->message;
        }
        result.error_message = err.empty() ? "Query execution failed" : err;
        if (pgWireDebugEnabled()) {
            std::fprintf(stderr,
                         "[pg_wire] executeRemoteQuery execute status=%d err=%s\n",
                         static_cast<int>(status),
                         result.error_message.c_str());
            std::fflush(stderr);
        }
        return status;
    }
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] executeRemoteQuery execute ok rows=%zu tag=%s\n",
                     static_cast<size_t>(rs.getRowCount()),
                     rs.getCommandTag().c_str());
        std::fflush(stderr);
    }

    result.columns.clear();
    for (const auto& col : rs.getColumns()) {
        ProtocolCodec::ColumnInfo info;
        info.name = col.name;
        info.type = col.type;
        info.type_modifier = col.type_modifier;
        result.columns.push_back(info);
    }

    result.rows.clear();
    const auto row_count = static_cast<size_t>(rs.getRowCount());
    for (size_t i = 0; i < row_count; ++i) {
        result.rows.push_back(rs.getRowValues(i));
    }

    result.rows_affected = rs.getRowsAffected();
    result.command_tag = rs.getCommandTag();
    if (result.command_tag.empty() || result.command_tag == "OK") {
        auto ltrim_upper = [](const std::string& input) {
            size_t pos = 0;
            while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
                ++pos;
            }
            std::string upper;
            upper.reserve(input.size() - pos);
            for (; pos < input.size(); ++pos) {
                upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(input[pos]))));
            }
            return upper;
        };
        std::string normalized = ltrim_upper(rewritten.query);
        auto starts_with = [&](const std::string& prefix) {
            return normalized.rfind(prefix, 0) == 0;
        };

        if (row_count > 0) {
            result.command_tag = "SELECT " + std::to_string(row_count);
        } else if (starts_with("INSERT")) {
            result.command_tag = "INSERT";
        } else if (starts_with("UPDATE")) {
            result.command_tag = "UPDATE";
        } else if (starts_with("DELETE")) {
            result.command_tag = "DELETE";
        } else if (starts_with("CREATE")) {
            result.command_tag = "CREATE";
        } else if (starts_with("DROP")) {
            result.command_tag = "DROP";
        } else if (starts_with("ALTER")) {
            result.command_tag = "ALTER";
        } else {
            result.command_tag = "OK";
        }
    }

    collectInheritsMergeNotices(client_.get(), rewritten.query, result.notices);

    auto append_notice = [&](const std::string& notice_text) {
        if (notice_text.empty()) {
            return;
        }
        for (const auto& existing : result.notices) {
            if (existing == notice_text) {
                return;
            }
        }
        result.notices.push_back(notice_text);
    };
    for (const auto& notice : rs.getNotices()) {
        append_notice(notice);
    }

    return core::Status::OK;
}

// ============================================================================
// Configuration
// ============================================================================

void PostgresqlAdapter::setServerParameter(const std::string& name, const std::string& value) {
    server_parameters_[name] = value;
}

void PostgresqlAdapter::setTLSConfig(const security::TLSConfig& config) {
    auto ctx = security::TLSContext::createServer(config);
    if (ctx) {
        tls_context_ = std::move(ctx);
        tls_enabled_ = true;
    }
}

void PostgresqlAdapter::initializeServerParameters() {
    // C1: server_version alignment - PostgreSQL format: "XX.X (ScratchBird XX.X)"
    // Report version as PostgreSQL 15.4 for compatibility
    server_parameters_["server_version"] = "15.4 (ScratchBird 1.0)";
    server_parameters_["server_encoding"] = "UTF8";
    server_parameters_["client_encoding"] = "UTF8";
    server_parameters_["DateStyle"] = "ISO, MDY";
    server_parameters_["TimeZone"] = "UTC";
    server_parameters_["integer_datetimes"] = "on";
    server_parameters_["standard_conforming_strings"] = "on";
    server_parameters_["application_name"] = "";
    server_parameters_["default_transaction_read_only"] = "off";
    server_parameters_["in_hot_standby"] = "off";
    server_parameters_["is_superuser"] = "on";
    server_parameters_["session_authorization"] = "";
    server_parameters_["dialect_tag"] = "scratchbird";
}

// ============================================================================
// ProtocolAdapter Implementation
// ============================================================================

core::Status PostgresqlAdapter::parseMessage(network::Connection* conn) {
    const auto& buffer = conn->getReadBuffer();

    if (pg_state_ == PgProtocolState::STARTUP) {
        // Startup message has no type byte, just length
        if (buffer.size() < 4) {
            return core::Status::IO_ERROR;  // Need more data
        }

        int32_t length = readInt32(buffer.data());
        if (length < 4 || length > 10000) {
            // Invalid startup message
            return core::Status::INVALID_ARGUMENT;
        }

        if (buffer.size() < static_cast<size_t>(length)) {
            return core::Status::IO_ERROR;  // Need more data
        }

        // Full startup message received
        current_msg_type_ = 0;  // No type for startup
        current_msg_length_ = length;
        current_msg_data_.assign(buffer.begin(), buffer.begin() + length);

        // Consume from read buffer
        conn->consumeReadBuffer(length);

        if (pgWireDebugEnabled()) {
            std::fprintf(stderr,
                         "[pg_wire] recv startup len=%d payload=%zu state=%d\n",
                         length,
                         current_msg_data_.size(),
                         static_cast<int>(pg_state_));
            std::fflush(stderr);
        }

        return core::Status::OK;
    }

    // Regular message: type (1 byte) + length (4 bytes) + payload
    if (buffer.size() < 5) {
        return core::Status::IO_ERROR;  // Need more data
    }

    current_msg_type_ = static_cast<char>(buffer[0]);
    current_msg_length_ = readInt32(buffer.data() + 1);

    if (current_msg_length_ < 4) {
        return core::Status::INVALID_ARGUMENT;
    }

    size_t total_length = 1 + static_cast<size_t>(current_msg_length_);
    if (buffer.size() < total_length) {
        return core::Status::IO_ERROR;  // Need more data
    }

    // Full message received
    current_msg_data_.assign(buffer.begin() + 5, buffer.begin() + total_length);

    // Consume from read buffer
    conn->consumeReadBuffer(total_length);

    if (pgWireDebugEnabled()) {
        const unsigned char type_u8 = static_cast<unsigned char>(current_msg_type_);
        const char printable_type =
            (type_u8 >= 32 && type_u8 <= 126) ? static_cast<char>(type_u8) : '?';
        std::fprintf(stderr,
                     "[pg_wire] recv type=%c(0x%02X) len=%d payload=%zu state=%d\n",
                     printable_type,
                     static_cast<unsigned>(type_u8),
                     current_msg_length_,
                     current_msg_data_.size(),
                     static_cast<int>(pg_state_));
        std::fflush(stderr);
    }

    return core::Status::OK;
}

core::Status PostgresqlAdapter::processMessage(network::Connection* conn) {
    bytes_received_ += current_msg_data_.size() + 5;

    if (pg_state_ == PgProtocolState::STARTUP) {
        return handleStartupMessage(conn);
    }

    if (sync_pending_ &&
        current_msg_type_ != pg::FrontendMsg::SYNC &&
        current_msg_type_ != pg::FrontendMsg::TERMINATE) {
        return core::Status::OK;
    }

    switch (current_msg_type_) {
        case pg::FrontendMsg::PASSWORD:
            return handlePasswordMessage(conn);

        case pg::FrontendMsg::QUERY:
            return handleQuery(conn);

        case pg::FrontendMsg::PARSE:
            return handleParse(conn);

        case pg::FrontendMsg::BIND:
            return handleBind(conn);

        case pg::FrontendMsg::DESCRIBE:
            return handleDescribe(conn);

        case pg::FrontendMsg::EXECUTE:
            return handleExecute(conn);

        case pg::FrontendMsg::CLOSE:
            return handleClose(conn);

        case pg::FrontendMsg::SYNC:
            return handleSync(conn);

        case pg::FrontendMsg::FLUSH:
            return handleFlush(conn);

        case pg::FrontendMsg::TERMINATE:
            return handleTerminate(conn);

        case pg::FrontendMsg::COPY_DATA:
            return handleCopyData(conn);

        case pg::FrontendMsg::COPY_DONE:
            return handleCopyDone(conn);

        case pg::FrontendMsg::COPY_FAIL:
            return handleCopyFail(conn);

        default:
            // Unknown message type
            sendErrorResponse(conn, "ERROR", "08P01",
                              "Unknown message type: " + std::string(1, current_msg_type_));
            return core::Status::INVALID_ARGUMENT;
    }
}

core::Status PostgresqlAdapter::sendGreeting(network::Connection* /*conn*/) {
    // PostgreSQL server doesn't send a greeting - it waits for client startup
    return core::Status::OK;
}

core::Status PostgresqlAdapter::processAuthentication(network::Connection* /*conn*/) {
    // Authentication is handled in handlePasswordMessage
    return core::Status::OK;
}

core::Status PostgresqlAdapter::sendAuthResult(network::Connection* conn,
                                                bool success,
                                                const std::string& error_msg) {
    if (success) {
        sendAuthenticationOk(conn);
        sendBackendKeyData(conn);

        // Send server parameters
        for (const auto& param : server_parameters_) {
            sendParameterStatus(conn, param.first, param.second);
        }

        sendReadyForQuery(conn);
        pg_state_ = PgProtocolState::READY;
    } else {
        sendErrorResponse(conn, "FATAL", "28P01", error_msg.empty() ? "Authentication failed" : error_msg);
    }

    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::sendQueryResult(network::Connection* conn,
                                                 const ResultContext& result) {
    if (result.has_error) {
        return sendProtocolError(conn, result.error_code, result.sqlstate,
                                 result.error_message, result.error_detail, result.error_hint);
    }

    for (const auto& notice : result.notices) {
        if (!notice.empty()) {
            sendNoticeResponse(conn, "NOTICE", notice);
        }
    }

    if (!result.columns.empty()) {
        // Send row description
        sendRowDescription(conn, result.columns);

        // Send rows if available
        for (const auto& row : result.rows) {
            sendDataRow(conn, result.columns, row);
        }
    }

    // Send command complete
    sendCommandComplete(conn, result.command_tag);

    return core::Status::OK;
}

core::Status PostgresqlAdapter::sendProtocolError(network::Connection* conn,
                                                   uint32_t error_code,
                                                   const std::string& sqlstate,
                                                   const std::string& message,
                                                   const std::string& detail,
                                                   const std::string& hint) {
    std::string mapped_state = sqlstate;
    mapStatusToSqlstate(error_code, mapped_state);
    sendErrorResponse(conn, "ERROR", mapped_state.empty() ? "XX000" : mapped_state, message, detail, hint);
    return core::Status::OK;
}

// ============================================================================
// Startup/Authentication Handling
// ============================================================================

core::Status PostgresqlAdapter::handleStartupMessage(network::Connection* conn) {
    // Parser workers are reused across client sessions; clear per-session
    // adapter state so auth/session context cannot leak between logins.
    if (client_) {
        client_->disconnect();
        client_.reset();
    }
    search_path_set_ = false;
    txn_failed_ = false;
    sync_pending_ = false;
    pending_operations_.clear();
    copy_context_ = CopyContext{};
    statements_.clear();
    portals_.clear();
    client_parameters_.clear();
    database_name_.clear();
    username_.clear();
    pg_schema_id_ = core::ID{};
    scram_step_ = 0;
    auth_method_ = AuthMethod::SCRAM_SHA_256;
    initializeServerParameters();

    if (current_msg_data_.size() < 8) {
        return core::Status::INVALID_ARGUMENT;
    }

    int32_t protocol_version = readInt32(current_msg_data_.data() + 4);

    // Check for special requests
    if (protocol_version == pg::SSL_REQUEST) {
        return handleSSLRequest(conn);
    }

    if (protocol_version == pg::GSSENC_REQUEST) {
        return handleGSSENCRequest(conn);
    }

    if (protocol_version == pg::CANCEL_REQUEST) {
        return handleCancelRequest(conn);
    }

    // Check protocol version
    if (protocol_version != pg::PROTOCOL_VERSION_3) {
        sendErrorResponse(conn, "FATAL", "08P01",
                          "Unsupported protocol version: " + std::to_string(protocol_version >> 16) +
                          "." + std::to_string(protocol_version & 0xFFFF));
        return core::Status::NOT_SUPPORTED;
    }

    // Parse startup parameters (null-terminated key-value pairs)
    size_t offset = 8;
    bool database_parameter_supplied = false;
    while (offset < current_msg_data_.size() - 1) {
        std::string key = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
        if (key.empty()) break;
        offset += key.size() + 1;

        if (offset >= current_msg_data_.size()) break;
        std::string value = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
        offset += value.size() + 1;

        client_parameters_[key] = value;

        // Extract important parameters
        if (key == "user") {
            username_ = value;
        } else if (key == "database") {
            database_parameter_supplied = true;
            database_name_ = value;
        }
    }

    auto normalize = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        return value;
    };

    if (username_.empty()) {
        sendErrorResponse(conn, "FATAL", "28000", "user name missing in startup packet");
        return core::Status::NOT_SUPPORTED;
    }

    auto repl_it = client_parameters_.find("replication");
    if (repl_it != client_parameters_.end()) {
        std::string repl = normalize(repl_it->second);
        if (!repl.empty() && repl != "0" && repl != "FALSE" && repl != "OFF") {
            sendErrorResponse(conn, "FATAL", "0A000", "Replication is not supported");
            return core::Status::NOT_SUPPORTED;
        }
    }

    auto enc_it = client_parameters_.find("client_encoding");
    if (enc_it != client_parameters_.end()) {
        std::string enc = normalize(enc_it->second);
        if (!enc.empty() && enc != "UTF8" && enc != "UTF-8") {
            sendErrorResponse(conn, "FATAL", "0A000", "Unsupported client_encoding");
            return core::Status::NOT_SUPPORTED;
        }
    }

    // In manager-bound sessions, a missing database token should resolve to the
    // bound default database, not to the username.
    if (database_name_.empty()) {
        if (!database_parameter_supplied &&
            config_.enforce_bound_database &&
            !config_.default_database.empty()) {
            database_name_ = config_.default_database;
        } else {
            database_name_ = username_;
        }
    }

    std::string selected_database;
    if (!resolveDatabaseSelection(database_name_, selected_database)) {
        if (config_.enforce_bound_database &&
            !config_.default_database.empty() &&
            isPostgresqlSystemDatabaseName(database_name_)) {
            selected_database = config_.default_database;
        } else {
            sendErrorResponse(conn, "FATAL", "28000",
                              "Database switch denied by manager binding context");
            return core::Status::INVALID_AUTHORIZATION;
        }
    }
    database_name_ = std::move(selected_database);
    client_parameters_["database"] = database_name_;
    client_parameters_["scratchbird.bound_database"] = database_name_;

    // Request authentication
    //
    // Emulation security policy boundary:
    // PostgreSQL lane resolves its own auth-method policy and does not
    // implicitly inherit non-PostgreSQL method variants.
    bool policy_auth_method_supported = true;
    auto resolvePostgresqlPolicyAuthMethod = [&]() -> AuthMethod {
        AuthMethod method = config_.auth_method;
        if (!scratchbird::security::parserAuthMethodSupported("postgresql", method)) {
            policy_auth_method_supported = false;
            return method;
        }
        if (method == AuthMethod::SCRAM_SHA_512) {
            // PostgreSQL wire policy normalizes SCRAM-512 to SCRAM-256.
            return AuthMethod::SCRAM_SHA_256;
        }
        return method;
    };

    if (config_.require_authentication) {
        auth_method_ = resolvePostgresqlPolicyAuthMethod();
        if (!policy_auth_method_supported) {
            sendErrorResponse(conn, "FATAL", "0A000",
                              "Configured authentication method is not supported by PostgreSQL emulation policy");
            return core::Status::NOT_SUPPORTED;
        }

        client_config_.manual_auth = true;

        if (auth_method_ == AuthMethod::MD5) {
            std::random_device rd;
            for (int i = 0; i < 4; ++i) {
                md5_salt_[i] = static_cast<uint8_t>(rd() & 0xFF);
            }
            sendAuthenticationMD5Password(conn, md5_salt_);
            pg_state_ = PgProtocolState::AUTH_MD5;
        } else if (auth_method_ == AuthMethod::SCRAM_SHA_256) {
            sendAuthenticationSASL(conn, {"SCRAM-SHA-256"});
            scram_step_ = 0;
            pg_state_ = PgProtocolState::AUTH_SCRAM;
        } else {
            sendAuthenticationCleartextPassword(conn);
            pg_state_ = PgProtocolState::AUTH_REQUESTED;
        }
    } else {
        // Trust authentication
        sendAuthResult(conn, true);
        pg_state_ = PgProtocolState::READY;
    }

    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::sendSSLResponse(network::Connection* conn, bool accept) {
    uint8_t response = accept ? 'S' : 'N';
    size_t written = 0;
    auto status = conn->getSocket()->write(&response, 1, &written);
    if (status != core::Status::OK || written != 1) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status PostgresqlAdapter::sendGSSENCResponse(network::Connection* conn, bool accept) {
    uint8_t response = accept ? 'G' : 'N';
    size_t written = 0;
    auto status = conn->getSocket()->write(&response, 1, &written);
    if (status != core::Status::OK || written != 1) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status PostgresqlAdapter::performTLSHandshake(network::Connection* conn) {
    if (!tls_context_) {
        return core::Status::INTERNAL_ERROR;
    }

    // Wrap the existing socket in a TLS connection
    tls_connection_ = std::make_unique<security::TLSConnection>(*tls_context_);
    
    auto status = tls_connection_->setFd(conn->getSocket()->getFd());
    if (status != core::Status::OK) {
        tls_connection_.reset();
        return status;
    }

    status = tls_connection_->accept();
    if (status != core::Status::OK) {
        tls_connection_.reset();
        return status;
    }

    tls_negotiated_ = true;
    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleSSLRequest(network::Connection* conn) {
    // C1: TLS support for SSLRequest
    // Check if TLS is configured and enabled
    if (!tls_enabled_ || !tls_context_) {
        // TLS not configured - send 'N' (no SSL)
        // This is a negotiated disable - client can continue with unencrypted connection
        auto status = sendSSLResponse(conn, false);
        if (status != core::Status::OK) {
            return status;
        }
        // Reset state to wait for another startup message
        pg_state_ = PgProtocolState::STARTUP;
        return core::Status::OK;
    }

    // TLS is available - send 'S' and perform handshake
    auto status = sendSSLResponse(conn, true);
    if (status != core::Status::OK) {
        return status;
    }

    // Perform TLS handshake
    status = performTLSHandshake(conn);
    if (status != core::Status::OK) {
        return status;
    }

    // Reset state to wait for startup message over TLS
    pg_state_ = PgProtocolState::STARTUP;
    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleGSSENCRequest(network::Connection* conn) {
    // C1: GSSENC handling - negotiated disable
    // GSSAPI encryption is not supported; send 'N' to allow client to continue
    // with alternative encryption (TLS) or unencrypted connection
    auto status = sendGSSENCResponse(conn, false);
    if (status != core::Status::OK) {
        return status;
    }

    // Reset state to wait for another startup message
    pg_state_ = PgProtocolState::STARTUP;
    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleCancelRequest(network::Connection* /*conn*/) {
    // Cancel request handling
    if (current_msg_data_.size() < 16) {
        return core::Status::INVALID_ARGUMENT;
    }

    int32_t pid = readInt32(current_msg_data_.data() + 8);
    int32_t key = readInt32(current_msg_data_.data() + 12);

    auto* target = findBackend(pid, key);
    if (target) {
        target->requestCancel();
    }

    return core::Status::OK;
}

core::Status PostgresqlAdapter::handlePasswordMessage(network::Connection* conn) {
    core::ErrorContext ctx;
    auto authFailureMessage =
        [&](const client::Connection::AuthResponse* auth_resp = nullptr) -> std::string {
        if (auth_resp && !auth_resp->error_message.empty()) {
            return auth_resp->error_message;
        }
        if (!ctx.message.empty()) {
            return ctx.message;
        }
        if (client_) {
            const std::string last_error = client_->getLastError();
            if (!last_error.empty()) {
                return last_error;
            }
        }
        return "Authentication failed for user \"" + username_ + "\"";
    };

    if (pg_state_ == PgProtocolState::AUTH_REQUESTED) {
        std::string password = readString(current_msg_data_.data(), current_msg_data_.size());
        std::vector<uint8_t> payload(password.begin(), password.end());
        auto status = connectRemoteClient(&ctx);
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] auth cleartext connect status=%d err=%s\n",
                     static_cast<int>(status),
                     ctx.message.c_str());
        std::fflush(stderr);
    }
        if (status != core::Status::OK) {
            return sendAuthResult(conn, false, authFailureMessage());
        }
        client::Connection::AuthResponse auth_resp;
        status = client_->sendAuthRequest(AuthMethod::PASSWORD, payload, auth_resp, &ctx);
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] auth cleartext request status=%d auth_status=%d err=%s\n",
                     static_cast<int>(status),
                     static_cast<int>(auth_resp.status),
                     auth_resp.error_message.c_str());
        std::fflush(stderr);
    }
        if (status != core::Status::OK || auth_resp.status != AuthStatus::OK) {
            return sendAuthResult(conn, false, authFailureMessage(&auth_resp));
        }
        return sendAuthResult(conn, true);
    }

    if (pg_state_ == PgProtocolState::AUTH_MD5) {
        std::string response = readString(current_msg_data_.data(), current_msg_data_.size());
        std::vector<uint8_t> payload;
        payload.insert(payload.end(), md5_salt_, md5_salt_ + 4);
        payload.insert(payload.end(), response.begin(), response.end());

        auto status = connectRemoteClient(&ctx);
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] auth md5 connect status=%d err=%s\n",
                     static_cast<int>(status),
                     ctx.message.c_str());
        std::fflush(stderr);
    }
        if (status != core::Status::OK) {
            return sendAuthResult(conn, false, authFailureMessage());
        }

        client::Connection::AuthResponse auth_resp;
        status = client_->sendAuthRequest(AuthMethod::MD5, payload, auth_resp, &ctx);
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] auth md5 request status=%d auth_status=%d err=%s\n",
                     static_cast<int>(status),
                     static_cast<int>(auth_resp.status),
                     auth_resp.error_message.c_str());
        std::fflush(stderr);
    }
        if (status != core::Status::OK || auth_resp.status != AuthStatus::OK) {
            return sendAuthResult(conn, false, authFailureMessage(&auth_resp));
        }
        return sendAuthResult(conn, true);
    }

    if (pg_state_ == PgProtocolState::AUTH_SCRAM) {
        if (scram_step_ == 0) {
            size_t offset = 0;
            std::string mechanism = readString(current_msg_data_.data() + offset,
                                                current_msg_data_.size() - offset);
            offset += mechanism.size() + 1;

            if (offset + 4 > current_msg_data_.size()) {
                sendErrorResponse(conn, "FATAL", "08P01", "Invalid SASL response");
                return sendBuffer(conn);
            }
            int32_t resp_len = readInt32(current_msg_data_.data() + offset);
            offset += 4;
            if (resp_len < 0 || offset + static_cast<size_t>(resp_len) > current_msg_data_.size()) {
                sendErrorResponse(conn, "FATAL", "08P01", "Invalid SASL response");
                return sendBuffer(conn);
            }

            std::string client_first(reinterpret_cast<const char*>(current_msg_data_.data() + offset),
                                     static_cast<size_t>(resp_len));
            client_first = normalizePostgresqlScramClientFirst(client_first, username_);

            if (mechanism != "SCRAM-SHA-256") {
                sendErrorResponse(conn, "FATAL", "0A000", "Unsupported SASL mechanism");
                return sendBuffer(conn);
            }
            if (client_first.rfind("p=", 0) == 0) {
                sendErrorResponse(conn, "FATAL", "0A000", "SCRAM-PLUS channel binding is not supported");
                return sendBuffer(conn);
            }

            auto status = connectRemoteClient(&ctx);
            if (pgWireDebugEnabled()) {
                std::fprintf(stderr,
                             "[pg_wire] auth scram step0 connect status=%d err=%s\n",
                             static_cast<int>(status),
                             ctx.message.c_str());
                std::fflush(stderr);
            }
            if (status != core::Status::OK) {
                return sendAuthResult(conn, false, authFailureMessage());
            }

            std::vector<uint8_t> payload(client_first.begin(), client_first.end());
            client::Connection::AuthResponse auth_resp;
            status = client_->sendAuthRequest(AuthMethod::SCRAM_SHA_256, payload, auth_resp, &ctx);
            if (pgWireDebugEnabled()) {
                std::fprintf(stderr,
                             "[pg_wire] auth scram step0 request status=%d auth_status=%d err=%s\n",
                             static_cast<int>(status),
                             static_cast<int>(auth_resp.status),
                             auth_resp.error_message.c_str());
                std::fflush(stderr);
            }
            if (status != core::Status::OK || auth_resp.status != AuthStatus::CONTINUE) {
                return sendAuthResult(conn, false, authFailureMessage(&auth_resp));
            }

            std::string server_first(auth_resp.data.begin(), auth_resp.data.end());
            sendAuthenticationSASLContinue(conn, server_first);
            scram_step_ = 1;
            return sendBuffer(conn);
        }

        if (scram_step_ == 1) {
            std::string client_final(reinterpret_cast<const char*>(current_msg_data_.data()),
                                     current_msg_data_.size());
            auto status = connectRemoteClient(&ctx);
            if (pgWireDebugEnabled()) {
                std::fprintf(stderr,
                             "[pg_wire] auth scram step1 connect status=%d err=%s\n",
                             static_cast<int>(status),
                             ctx.message.c_str());
                std::fflush(stderr);
            }
            if (status != core::Status::OK) {
                return sendAuthResult(conn, false, authFailureMessage());
            }

            std::vector<uint8_t> payload(client_final.begin(), client_final.end());
            client::Connection::AuthResponse auth_resp;
            status = client_->sendAuthRequest(AuthMethod::SCRAM_SHA_256, payload, auth_resp, &ctx);
            if (pgWireDebugEnabled()) {
                std::fprintf(stderr,
                             "[pg_wire] auth scram step1 request status=%d auth_status=%d err=%s\n",
                             static_cast<int>(status),
                             static_cast<int>(auth_resp.status),
                             auth_resp.error_message.c_str());
                std::fflush(stderr);
            }
            scram_step_ = 0;
            if (status != core::Status::OK || auth_resp.status != AuthStatus::OK) {
                return sendAuthResult(conn, false, authFailureMessage(&auth_resp));
            }

            std::string server_final(auth_resp.data.begin(), auth_resp.data.end());
            sendAuthenticationSASLFinal(conn, server_final);
            return sendAuthResult(conn, true);
        }
    }

    return core::Status::INTERNAL_ERROR;
}

// ============================================================================
// Query Handling
// ============================================================================

core::Status PostgresqlAdapter::handleQuery(network::Connection* conn) {
    // Simple query protocol
    std::string query = readString(current_msg_data_.data(), current_msg_data_.size());
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] handleQuery sql=%s\n",
                     query.c_str());
        std::fflush(stderr);
    }

    // Check for empty query
    if (query.empty() || query == ";") {
        sendEmptyQueryResponse(conn);
        sendReadyForQuery(conn);
        return sendBuffer(conn);
    }

    ResultContext show_result;
    if (tryBuildShowServerParameterResult(query, server_parameters_, show_result)) {
        updateTransactionStatus(query, false);
        sendQueryResult(conn, show_result);
        sendReadyForQuery(conn);
        return sendBuffer(conn);
    }

    CopyContext copy_ctx;
    std::string copy_error;
    if (parseCopyQuery(query, copy_ctx, copy_error)) {
        if (!copy_error.empty()) {
            sendErrorResponse(conn, "ERROR", "0A000", copy_error);
            sendReadyForQuery(conn);
            return sendBuffer(conn);
        }
        copy_ctx.from_extended = false;
        if (copy_ctx.from_stdin) {
            return startCopyIn(conn, copy_ctx);
        }
        if (copy_ctx.to_stdout) {
            return startCopyOut(conn, copy_ctx);
        }
        sendErrorResponse(conn, "ERROR", "0A000", "COPY only supports STDIN/STDOUT");
        sendReadyForQuery(conn);
        return sendBuffer(conn);
    }

    // Execute query
    QueryContext ctx;
    ctx.query = query;

    ResultContext result;
    auto status = executeRemoteQuery(ctx, result);
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] handleQuery executeRemoteQuery status=%d has_error=%d code=%u msg=%s\n",
                     static_cast<int>(status),
                     result.has_error ? 1 : 0,
                     result.error_code,
                     result.error_message.c_str());
        std::fflush(stderr);
    }

    updateTransactionStatus(query, result.has_error);

    if (result.has_error) {
        sendProtocolError(conn, result.error_code, result.sqlstate,
                  result.error_message, result.error_detail, result.error_hint);
    } else {
        sendQueryResult(conn, result);
    }

    sendReadyForQuery(conn);
    return sendBuffer(conn);
}

// ============================================================================
// Extended Query Protocol
// ============================================================================

core::Status PostgresqlAdapter::handleParse(network::Connection* conn) {
    size_t offset = 0;

    // Statement name (empty = unnamed)
    std::string stmt_name = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
    offset += stmt_name.size() + 1;

    // Query string
    std::string query = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
    offset += query.size() + 1;

    // Number of parameter types
    if (offset + 2 > current_msg_data_.size()) {
        sync_pending_ = true;
        sendErrorResponse(conn, "ERROR", "08P01", "Invalid Parse message");
        return core::Status::INVALID_ARGUMENT;
    }
    int16_t num_params = readInt16(current_msg_data_.data() + offset);
    offset += 2;

    // Parameter OIDs
    std::vector<int32_t> param_types;
    for (int16_t i = 0; i < num_params; ++i) {
        if (offset + 4 > current_msg_data_.size()) break;
        param_types.push_back(readInt32(current_msg_data_.data() + offset));
        offset += 4;
    }

    // Store prepared statement
    PgPreparedStatement stmt;
    stmt.name = stmt_name;
    stmt.query = query;
    stmt.param_types = param_types;
    statements_[stmt_name] = stmt;

    sendParseComplete(conn);
    pending_operations_.push_back('P');

    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleBind(network::Connection* conn) {
    size_t offset = 0;

    // Portal name (empty = unnamed)
    std::string portal_name = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
    offset += portal_name.size() + 1;

    // Statement name
    std::string stmt_name = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
    offset += stmt_name.size() + 1;

    // Check statement exists
    auto stmt_it = statements_.find(stmt_name);
    if (stmt_it == statements_.end()) {
        sync_pending_ = true;
        sendErrorResponse(conn, "ERROR", "26000", "Prepared statement does not exist: " + stmt_name);
        return core::Status::NOT_FOUND;
    }

    // Number of parameter format codes
    if (offset + 2 > current_msg_data_.size()) {
        sync_pending_ = true;
        sendErrorResponse(conn, "ERROR", "08P01", "Invalid Bind message");
        return core::Status::INVALID_ARGUMENT;
    }
    int16_t num_format_codes = readInt16(current_msg_data_.data() + offset);
    offset += 2;

    // Format codes
    std::vector<int16_t> param_formats;
    for (int16_t i = 0; i < num_format_codes; ++i) {
        if (offset + 2 > current_msg_data_.size()) {
            sync_pending_ = true;
            sendErrorResponse(conn, "ERROR", "08P01", "Invalid Bind message");
            return core::Status::INVALID_ARGUMENT;
        }
        int16_t format_code = readInt16(current_msg_data_.data() + offset);
        if (format_code != 1) {
            format_code = 0;
        }
        param_formats.push_back(format_code);
        offset += 2;
    }

    // Number of parameters
    if (offset + 2 > current_msg_data_.size()) {
        sync_pending_ = true;
        sendErrorResponse(conn, "ERROR", "08P01", "Invalid Bind message");
        return core::Status::INVALID_ARGUMENT;
    }
    int16_t num_params = readInt16(current_msg_data_.data() + offset);
    offset += 2;

    // Parameter values
    PgPortal portal;
    portal.name = portal_name;
    portal.statement_name = stmt_name;
    portal.fetch_pos = 0;
    portal.buffered_rows.clear();
    portal.more_rows_available = false;

    for (int16_t i = 0; i < num_params; ++i) {
        int16_t fmt = 0;
        if (!param_formats.empty()) {
            if (param_formats.size() == 1) {
                fmt = param_formats.front();
            } else if (static_cast<size_t>(i) < param_formats.size()) {
                fmt = param_formats[i];
            }
        }
        if (offset + 4 > current_msg_data_.size()) {
            sync_pending_ = true;
            sendErrorResponse(conn, "ERROR", "08P01", "Invalid Bind message");
            return core::Status::INVALID_ARGUMENT;
        }
        int32_t len = readInt32(current_msg_data_.data() + offset);
        offset += 4;

        if (len == -1) {
            // NULL
            portal.param_values.emplace_back("");
            portal.param_nulls.push_back(true);
            portal.param_formats.push_back(fmt);
        } else {
            // Value
            if (len < 0 || offset + static_cast<size_t>(len) > current_msg_data_.size()) {
                sync_pending_ = true;
                sendErrorResponse(conn, "ERROR", "08P01", "Invalid Bind message");
                return core::Status::INVALID_ARGUMENT;
            }
            WireType ptype = WireType::UNKNOWN;
            if (static_cast<size_t>(i) < stmt_it->second.param_types.size()) {
                ptype = oidToWireType(stmt_it->second.param_types[i]);
            }
            if (fmt == 1) {
                std::string text_val = decodeBinaryParamToText(
                    ptype,
                    current_msg_data_.data() + offset,
                    static_cast<size_t>(len));
                portal.param_values.emplace_back(std::move(text_val));
            } else {
                portal.param_values.emplace_back(
                    reinterpret_cast<const char*>(current_msg_data_.data() + offset), len);
            }
            portal.param_nulls.push_back(false);
            portal.param_formats.push_back(fmt);
            offset += len;
        }
    }

    // Result format codes
    if (offset + 2 <= current_msg_data_.size()) {
        int16_t num_result_formats = readInt16(current_msg_data_.data() + offset);
        offset += 2;

        for (int16_t i = 0; i < num_result_formats; ++i) {
            if (offset + 2 > current_msg_data_.size()) {
                sync_pending_ = true;
                sendErrorResponse(conn, "ERROR", "08P01", "Invalid Bind message");
                return core::Status::INVALID_ARGUMENT;
            }
            int16_t format_code = readInt16(current_msg_data_.data() + offset);
            if (format_code != 1) {
                format_code = 0;
            }
            portal.result_formats.push_back(format_code);
            offset += 2;
        }
    }

    portals_[portal_name] = portal;

    sendBindComplete(conn);
    pending_operations_.push_back('B');

    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleDescribe(network::Connection* conn) {
    if (current_msg_data_.empty()) {
        sync_pending_ = true;
        sendErrorResponse(conn, "ERROR", "08P01", "Invalid Describe message");
        return core::Status::INVALID_ARGUMENT;
    }

    char type = static_cast<char>(current_msg_data_[0]);  // 'S' for statement, 'P' for portal
    std::string name = readString(current_msg_data_.data() + 1, current_msg_data_.size() - 1);

    if (type == 'S') {
        // Describe statement
        auto it = statements_.find(name);
        if (it == statements_.end()) {
            sync_pending_ = true;
            sendErrorResponse(conn, "ERROR", "26000", "Prepared statement does not exist: " + name);
            return core::Status::NOT_FOUND;
        }

        // Send parameter description
        sendParameterDescription(conn, it->second.param_types);

        // Send row description (if it's a SELECT)
        if (it->second.result_columns.empty()) {
            sendNoData(conn);
        } else {
            sendRowDescription(conn, it->second.result_columns);
        }
    } else if (type == 'P') {
        // Describe portal
        auto it = portals_.find(name);
        if (it == portals_.end()) {
            sync_pending_ = true;
            sendErrorResponse(conn, "ERROR", "34000", "Portal does not exist: " + name);
            return core::Status::NOT_FOUND;
        }

        // Get statement for this portal
        auto stmt_it = statements_.find(it->second.statement_name);
        if (stmt_it == statements_.end()) {
            sendNoData(conn);
        } else if (stmt_it->second.result_columns.empty()) {
            sendNoData(conn);
        } else {
            sendRowDescription(conn, stmt_it->second.result_columns, it->second.result_formats);
        }
    }

    pending_operations_.push_back('D');
    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleExecute(network::Connection* conn) {
    // Portal name
    std::string portal_name = readString(current_msg_data_.data(), current_msg_data_.size());
    size_t offset = portal_name.size() + 1;

    // Max rows
    int32_t max_rows = 0;
    if (offset + 4 <= current_msg_data_.size()) {
        max_rows = readInt32(current_msg_data_.data() + offset);
    }

    // Find portal
    auto it = portals_.find(portal_name);
    if (it == portals_.end()) {
        sync_pending_ = true;
        sendErrorResponse(conn, "ERROR", "34000", "Portal does not exist: " + portal_name);
        return core::Status::NOT_FOUND;
    }

    PgPortal& portal = it->second;
    // Find statement
    auto stmt_it = statements_.find(portal.statement_name);
    if (stmt_it == statements_.end()) {
        sync_pending_ = true;
        sendErrorResponse(conn, "ERROR", "26000", "Prepared statement does not exist");
        return core::Status::NOT_FOUND;
    }

    // If this portal already executed, reuse buffered rows instead of re-executing
    if (portal.executed) {
        if (!portal.buffered_rows.empty() && portal.fetch_pos < portal.buffered_rows.size()) {
            size_t remaining = portal.buffered_rows.size() - portal.fetch_pos;
            size_t to_send = max_rows > 0
                ? std::min<size_t>(static_cast<size_t>(max_rows), remaining)
                : remaining;

            if (!stmt_it->second.result_columns.empty()) {
                for (size_t i = 0; i < to_send; ++i) {
                    sendDataRow(conn, stmt_it->second.result_columns,
                                portal.buffered_rows[portal.fetch_pos + i],
                                portal.result_formats);
                }
            }

            portal.fetch_pos += to_send;
            if (portal.fetch_pos < portal.buffered_rows.size()) {
                sendPortalSuspended(conn);
            } else {
                std::string tag = portal.command_tag.empty()
                    ? "SELECT " + std::to_string(portal.fetch_pos)
                    : portal.command_tag;
                sendCommandComplete(conn, tag);
                portal.completed = true;
            }
            pending_operations_.push_back('E');
            return sendBuffer(conn);
        }

        // Already executed and drained: acknowledge completion without rerun
        std::string tag = portal.command_tag.empty()
            ? "SELECT " + std::to_string(portal.fetch_pos)
            : portal.command_tag;
        sendCommandComplete(conn, tag);
        pending_operations_.push_back('E');
        portal.completed = true;
        return sendBuffer(conn);
    }

    CopyContext copy_ctx;
    std::string copy_error;
    if (parseCopyQuery(stmt_it->second.query, copy_ctx, copy_error)) {
        if (!copy_error.empty()) {
            sync_pending_ = true;
            sendErrorResponse(conn, "ERROR", "0A000", copy_error);
            pending_operations_.push_back('E');
            return core::Status::OK;
        }
        copy_ctx.from_extended = true;
        copy_ctx.portal_name = portal_name;
        copy_ctx.statement_name = portal.statement_name;
        pending_operations_.push_back('E');
        if (copy_ctx.from_stdin) {
            return startCopyIn(conn, copy_ctx);
        }
        if (copy_ctx.to_stdout) {
            return startCopyOut(conn, copy_ctx);
        }
        sync_pending_ = true;
        sendErrorResponse(conn, "ERROR", "0A000", "COPY only supports STDIN/STDOUT");
        return core::Status::OK;
    }

    // Execute
    QueryContext ctx;
    ctx.query = stmt_it->second.query;
    ctx.statement_name = portal.statement_name;
    ctx.portal_name = portal_name;
    ctx.parameter_values = portal.param_values;
    ctx.parameter_nulls = portal.param_nulls;
    ctx.parameter_formats.clear();
    ctx.parameter_formats.reserve(portal.param_formats.size());
    for (auto f : portal.param_formats) {
        ctx.parameter_formats.push_back(static_cast<int32_t>(f));
    }
    ctx.result_formats = portal.result_formats;
    ctx.max_rows = max_rows;

    ResultContext result;
    result.row_callback = [&](const std::vector<ProtocolCodec::ColumnValue>& row) {
        // Buffer rows when a max_rows limit is present
        if (max_rows > 0) {
            if (portal.buffered_rows.size() < static_cast<size_t>(max_rows)) {
                portal.buffered_rows.push_back(row);
            }
            // Stop fetching once we've buffered the requested slice
            if (portal.buffered_rows.size() >= static_cast<size_t>(max_rows)) {
                portal.more_rows_available = true;
                return false;
            }
            return true;
        }
        if (!result.columns.empty()) {
            sendDataRow(conn, result.columns, row, portal.result_formats);
        }
        return true;
    };
    executeRemoteQuery(ctx, result);

    updateTransactionStatus(stmt_it->second.query, result.has_error);

    if (result.has_error) {
        sync_pending_ = true;
        sendProtocolError(conn, result.error_code, result.sqlstate,
                  result.error_message, result.error_detail, result.error_hint);
    } else {
        for (const auto& notice : result.notices) {
            if (!notice.empty()) {
                sendNoticeResponse(conn, "NOTICE", notice);
            }
        }
        if (portal.buffered_rows.empty() && !result.rows.empty() && max_rows > 0) {
            portal.buffered_rows = result.rows;
        }
        if (!result.columns.empty()) {
            sendRowDescription(conn, result.columns, portal.result_formats);
            // Send buffered rows according to portal fetch state
            if (max_rows > 0) {
                const auto& source_rows = portal.buffered_rows.empty() ? result.rows : portal.buffered_rows;
                size_t remaining = source_rows.size() > portal.fetch_pos
                    ? source_rows.size() - portal.fetch_pos
                    : 0;
                size_t to_send = std::min<size_t>(static_cast<size_t>(max_rows), remaining);
                for (size_t i = 0; i < to_send; ++i) {
                    const auto& row = source_rows[portal.fetch_pos + i];
                    sendDataRow(conn, result.columns, row, portal.result_formats);
                }
                portal.fetch_pos += to_send;
                if (portal.more_rows_available || remaining > static_cast<size_t>(max_rows)) {
                    sendPortalSuspended(conn);
                } else {
                    sendCommandComplete(conn, result.command_tag);
                }
            } else {
                const auto& source_rows = portal.buffered_rows.empty() ? result.rows : portal.buffered_rows;
                for (const auto& row : source_rows) {
                    sendDataRow(conn, result.columns, row, portal.result_formats);
                }
                portal.fetch_pos = source_rows.size();
                sendCommandComplete(conn, result.command_tag);
            }
        } else {
            sendCommandComplete(conn, result.command_tag);
        }
        if (stmt_it->second.result_columns.empty() && !result.columns.empty()) {
            stmt_it->second.result_columns = result.columns;
        }
    }

    portal.executed = true;
    portal.command_tag = result.command_tag;
    // If we suspended due to max_rows, keep the portal open for fetches
    if (!portal.more_rows_available && portal.fetch_pos >= portal.buffered_rows.size()) {
        portal.completed = true;
    }
    pending_operations_.push_back('E');

    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleFetch(network::Connection* conn) {
    // Treat fetch as an execute against an already-bound portal
    // Portal name
    std::string portal_name = readString(current_msg_data_.data(), current_msg_data_.size());
    size_t offset = portal_name.size() + 1;

    // Max rows (optional)
    int32_t max_rows = 0;
    if (offset + 4 <= current_msg_data_.size()) {
        max_rows = readInt32(current_msg_data_.data() + offset);
    }

    auto it = portals_.find(portal_name);
    if (it == portals_.end()) {
        sync_pending_ = true;
        sendErrorResponse(conn, "ERROR", "34000", "Portal does not exist: " + portal_name);
        return core::Status::NOT_FOUND;
    }

    PgPortal& portal = it->second;
    auto stmt_it = statements_.find(portal.statement_name);
    if (stmt_it == statements_.end()) {
        sync_pending_ = true;
        sendErrorResponse(conn, "ERROR", "26000", "Prepared statement does not exist");
        return core::Status::NOT_FOUND;
    }
    // Reuse existing buffered rows
    size_t remaining = portal.buffered_rows.size() > portal.fetch_pos
        ? portal.buffered_rows.size() - portal.fetch_pos
        : 0;
    size_t to_send = max_rows > 0
        ? std::min<size_t>(static_cast<size_t>(max_rows), remaining)
        : remaining;

    if (!portal.buffered_rows.empty()) {
        // Only send rows; RowDescription has already been sent
        for (size_t i = 0; i < to_send; ++i) {
            sendDataRow(conn, stmt_it->second.result_columns,
                        portal.buffered_rows[portal.fetch_pos + i],
                        portal.result_formats);
        }
    }

    portal.fetch_pos += to_send;
    if (portal.fetch_pos < portal.buffered_rows.size() || portal.more_rows_available) {
        sendPortalSuspended(conn);
    } else {
        std::string tag = "SELECT " + std::to_string(portal.fetch_pos);
        sendCommandComplete(conn, tag);
        portal.completed = true;
        portal.more_rows_available = false;
    }
    pending_operations_.push_back('E');
    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::handleClose(network::Connection* conn) {
    if (current_msg_data_.empty()) {
        sendErrorResponse(conn, "ERROR", "08P01", "Invalid Close message");
        return core::Status::INVALID_ARGUMENT;
    }

    char type = static_cast<char>(current_msg_data_[0]);
    std::string name = readString(current_msg_data_.data() + 1, current_msg_data_.size() - 1);

    if (type == 'S') {
        statements_.erase(name);
    } else if (type == 'P') {
        portals_.erase(name);
    }

    sendCloseComplete(conn);
    pending_operations_.push_back('C');

    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleSync(network::Connection* conn) {
    // Sync marks end of extended query batch
    pending_operations_.clear();
    sync_pending_ = false;

    sendReadyForQuery(conn);
    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::handleFlush(network::Connection* conn) {
    // Flush sends all pending output
    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::handleCopyData(network::Connection* conn) {
    if (!copy_context_.active || !copy_context_.from_stdin) {
        sendErrorResponse(conn, "ERROR", "08P01", "COPY DATA without active COPY IN");
        if (!copy_context_.from_extended) {
            sendReadyForQuery(conn);
        }
        return sendBuffer(conn);
    }
    copy_context_.bytes += current_msg_data_.size();
    copy_context_.buffer.append(reinterpret_cast<const char*>(current_msg_data_.data()),
                                current_msg_data_.size());
    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleCopyDone(network::Connection* conn) {
    if (!copy_context_.active || !copy_context_.from_stdin) {
        sendErrorResponse(conn, "ERROR", "08P01", "COPY DONE without active COPY IN");
        if (!copy_context_.from_extended) {
            sendReadyForQuery(conn);
        }
        return sendBuffer(conn);
    }
    return finishCopyIn(conn);
}

core::Status PostgresqlAdapter::handleCopyFail(network::Connection* conn) {
    std::string message = readString(current_msg_data_.data(), current_msg_data_.size());
    if (message.empty()) {
        message = "COPY failed";
    }
    if (copy_context_.active) {
        std::string direction = copy_context_.from_stdin ? "from" : "to";
        recordCopyMetrics(direction, copy_context_.rows, copy_context_.bytes, true,
                          copy_context_.start_time);
    }
    bool from_extended = copy_context_.from_extended;
    sendErrorResponse(conn, "ERROR", "57014", message);
    copy_context_ = CopyContext{};
    pg_state_ = PgProtocolState::READY;
    if (!from_extended) {
        sendReadyForQuery(conn);
    }
    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::handleTerminate(network::Connection* conn) {
    if (client_) {
        client_->disconnect();
        client_.reset();
    }
    search_path_set_ = false;
    sync_pending_ = false;
    pending_operations_.clear();
    copy_context_ = CopyContext{};
    statements_.clear();
    portals_.clear();
    client_parameters_.clear();
    database_name_.clear();
    username_.clear();
    pg_schema_id_ = core::ID{};
    scram_step_ = 0;
    txn_failed_ = false;
    pg_state_ = PgProtocolState::CLOSING;
    conn->close(network::CloseReason::CLIENT_DISCONNECT);
    return core::Status::OK;
}

void PostgresqlAdapter::recordCopyMetrics(const std::string& direction,
                                          uint64_t rows,
                                          uint64_t bytes,
                                          bool error,
                                          const std::chrono::steady_clock::time_point& start_time) {
    auto& metrics = core::ScratchBirdMetrics::getInstance();
    metrics.initialize();

    if (metrics.copy_rows_total && rows > 0) {
        metrics.copy_rows_total->inc(static_cast<double>(rows), {direction});
    }
    if (metrics.copy_bytes_total && bytes > 0) {
        metrics.copy_bytes_total->inc(static_cast<double>(bytes), {direction});
    }
    if (metrics.copy_errors_total && error) {
        metrics.copy_errors_total->inc(1.0);
    }
    if (metrics.copy_duration_seconds) {
        auto end_time = std::chrono::steady_clock::now();
        double duration_seconds =
            std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time).count();
        metrics.copy_duration_seconds->observe(duration_seconds, {direction});
    }
}

// ============================================================================
// Message Sending
// ============================================================================

void PostgresqlAdapter::sendMessage(network::Connection* conn, char type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> msg;
    msg.reserve(1 + 4 + payload.size());

    // Type
    writeByte(msg, static_cast<uint8_t>(type));

    // Length (includes itself but not type)
    writeInt32(msg, static_cast<int32_t>(4 + payload.size()));

    // Payload
    writeBytes(msg, payload.data(), payload.size());

    writeToBuffer(conn, msg.data(), msg.size());
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] send type=%c(0x%02X) payload=%zu total=%zu state=%d\n",
                     std::isprint(static_cast<unsigned char>(type)) ? type : '?',
                     static_cast<unsigned int>(static_cast<unsigned char>(type)),
                     payload.size(),
                     msg.size(),
                     static_cast<int>(pg_state_));
        std::fflush(stderr);
    }
}

void PostgresqlAdapter::sendAuthenticationOk(network::Connection* conn) {
    std::vector<uint8_t> payload;
    writeInt32(payload, pg::AuthType::OK);
    sendMessage(conn, pg::BackendMsg::AUTHENTICATION, payload);
}

void PostgresqlAdapter::sendAuthenticationMD5Password(network::Connection* conn, const uint8_t salt[4]) {
    std::vector<uint8_t> payload;
    writeInt32(payload, pg::AuthType::MD5_PASSWORD);
    writeBytes(payload, salt, 4);
    sendMessage(conn, pg::BackendMsg::AUTHENTICATION, payload);
}

void PostgresqlAdapter::sendAuthenticationCleartextPassword(network::Connection* conn) {
    std::vector<uint8_t> payload;
    writeInt32(payload, pg::AuthType::CLEARTEXT_PASSWORD);
    sendMessage(conn, pg::BackendMsg::AUTHENTICATION, payload);
}

void PostgresqlAdapter::sendAuthenticationSASL(network::Connection* conn,
                                               const std::vector<std::string>& mechanisms) {
    std::vector<uint8_t> payload;
    writeInt32(payload, pg::AuthType::SASL);
    for (const auto& mech : mechanisms) {
        writeString(payload, mech);
    }
    writeByte(payload, 0);
    sendMessage(conn, pg::BackendMsg::AUTHENTICATION, payload);
}

void PostgresqlAdapter::sendAuthenticationSASLContinue(network::Connection* conn, const std::string& data) {
    std::vector<uint8_t> payload;
    writeInt32(payload, pg::AuthType::SASL_CONTINUE);
    writeBytes(payload, data.data(), data.size());
    sendMessage(conn, pg::BackendMsg::AUTHENTICATION, payload);
}

void PostgresqlAdapter::sendAuthenticationSASLFinal(network::Connection* conn, const std::string& data) {
    std::vector<uint8_t> payload;
    writeInt32(payload, pg::AuthType::SASL_FINAL);
    writeBytes(payload, data.data(), data.size());
    sendMessage(conn, pg::BackendMsg::AUTHENTICATION, payload);
}

void PostgresqlAdapter::sendBackendKeyData(network::Connection* conn) {
    std::vector<uint8_t> payload;
    writeInt32(payload, backend_pid_);
    writeInt32(payload, backend_secret_key_);
    sendMessage(conn, pg::BackendMsg::BACKEND_KEY_DATA, payload);
}

void PostgresqlAdapter::sendParameterStatus(network::Connection* conn,
                                             const std::string& name,
                                             const std::string& value) {
    std::vector<uint8_t> payload;
    writeString(payload, name);
    writeString(payload, value);
    sendMessage(conn, pg::BackendMsg::PARAMETER_STATUS, payload);
}

void PostgresqlAdapter::sendReadyForQuery(network::Connection* conn) {
    std::vector<uint8_t> payload;
    char status = pg::TransactionStatus::IDLE;
    if (txn_failed_) {
        status = pg::TransactionStatus::FAILED;
    } else if (in_transaction_) {
        status = pg::TransactionStatus::IN_TRANSACTION;
    }
    writeByte(payload, static_cast<uint8_t>(status));
    sendMessage(conn, pg::BackendMsg::READY_FOR_QUERY, payload);
}

core::Status PostgresqlAdapter::ensurePostgresSystemCatalog(core::ErrorContext* ctx) {
    if (!engineDatabase()) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Database not initialized");
        return core::Status::INVALID_ARGUMENT;
    }

    auto* catalog = engineDatabase()->catalog_manager();
    if (!catalog) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Catalog manager not available");
        return core::Status::INVALID_ARGUMENT;
    }

    std::string db_name = resolveEmulatedDatabaseForSession(client_parameters_, database_name_);
    if (db_name.empty() && !database_path_.empty()) {
        auto stem = std::filesystem::path(database_path_).stem().string();
        if (!stem.empty()) {
            db_name = stem;
        }
    }
    if (db_name.empty()) {
        db_name = "default";
    }
    db_name = extractEmulatedDatabaseName(db_name);
    if (db_name.empty()) {
        db_name = "default";
    }

    std::string schema_name = resolvePostgresqlSchemaPath(catalog, db_name);
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] ensurePostgresSystemCatalog db_name=%s schema_name=%s\n",
                     db_name.c_str(),
                     schema_name.c_str());
        std::fflush(stderr);
    }

    core::CatalogManager::SchemaInfo schema_info;
    auto status = catalog->getSchema(schema_name, schema_info, ctx);
    if (status != core::Status::OK) {
        if (status != core::Status::INVALID_ARGUMENT && status != core::Status::NOT_FOUND) {
            return status;
        }
        if (schema_name != buildLegacyPostgresqlSchemaPath(db_name)) {
            core::ErrorContext legacy_ctx;
            std::string legacy_schema = buildLegacyPostgresqlSchemaPath(db_name);
            if (catalog->getSchema(legacy_schema, schema_info, &legacy_ctx) == core::Status::OK) {
                schema_name = legacy_schema;
                status = core::Status::OK;
            }
        }
        if (status != core::Status::OK) {
            core::ID schema_id;
            status = catalog->createSchemaPath(schema_name,
                                               core::CatalogManager::SchemaType::REMOTE_EMULATED,
                                               schema_id,
                                               ctx);
            if (status != core::Status::OK) {
                return status;
            }
            status = catalog->getSchema(schema_id, schema_info, ctx);
            if (status != core::Status::OK) {
                return status;
            }
        }
    }
    pg_schema_id_ = schema_info.schema_id;

    auto ensure_view = [&](const std::string& name, const std::string& definition) -> core::Status {
        // Always replace stale bootstrap placeholders so emulation metadata
        // reflects current virtual catalog surfaces.
        return catalog->createView(schema_info.schema_id, name, definition, true,
                                   false, false, {}, core::ID{}, ctx);
    };

    // Compatibility bridge views for PostgreSQL system-catalog aliases.
    // Queries compiled against the emulated root schema are redirected here,
    // while the backing data remains sourced from virtual pg_catalog handlers.
    status = ensure_view("pg_database",
                         "SELECT d.datname AS datname, d.oid AS oid, d.encoding AS encoding, "
                         "NULL::text AS datcollate, NULL::text AS datctype, "
                         "NULL::boolean AS datistemplate, NULL::boolean AS datallowconn "
                         "FROM pg_catalog.pg_database d");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_namespace",
                         "SELECT n.nspname AS nspname, n.oid AS oid, "
                         "n.nspowner AS nspowner, n.nspacl AS nspacl "
                         "FROM pg_catalog.pg_namespace n");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_class",
                         "SELECT c.relname AS relname, c.oid AS oid, "
                         "c.relnamespace AS relnamespace, c.relkind AS relkind, "
                         "c.relowner AS relowner, c.reltablespace AS reltablespace, "
                         "c.relfilenode AS relfilenode, c.reltoastrelid AS reltoastrelid, "
                         "c.relpersistence AS relpersistence, c.relhasindex AS relhasindex, "
                         "c.relpages AS relpages, c.reltuples AS reltuples, "
                         "c.relnatts AS relnatts "
                         "FROM pg_catalog.pg_class c");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_attribute",
                         "SELECT a.attname AS attname, a.attrelid AS attrelid, "
                         "a.attnum AS attnum, a.atttypid AS atttypid, "
                         "a.attnotnull AS attnotnull, a.attisdropped AS attisdropped, "
                         "a.atttypmod AS atttypmod "
                         "FROM pg_catalog.pg_attribute a");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_type",
                         "SELECT t.typname AS typname, t.oid AS oid, "
                         "t.typnamespace AS typnamespace, t.typlen AS typlen, "
                         "t.typtype AS typtype FROM pg_catalog.pg_type t");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_am",
                         "SELECT a.oid AS oid, a.amname AS amname, "
                         "a.amhandler AS amhandler, a.amtype AS amtype "
                         "FROM pg_catalog.pg_am a");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_roles",
                         "SELECT r.rolname AS rolname, r.rolsuper AS rolsuper, "
                         "r.rolcreaterole AS rolcreaterole, r.rolcreatedb AS rolcreatedb, "
                         "NULL::boolean AS rolinherit, r.rolcanlogin AS rolcanlogin, "
                         "r.rolreplication AS rolreplication "
                         "FROM pg_catalog.pg_roles r");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_authid",
                         "SELECT a.rolname AS rolname, a.rolsuper AS rolsuper, "
                         "a.rolcreaterole AS rolcreaterole, a.rolcreatedb AS rolcreatedb, "
                         "NULL::boolean AS rolinherit, a.rolcanlogin AS rolcanlogin, "
                         "a.rolreplication AS rolreplication "
                         "FROM pg_catalog.pg_authid a");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_proc",
                         "SELECT p.proname AS proname, p.pronamespace AS pronamespace, "
                         "p.proowner AS proowner, NULL::bigint AS prolang, "
                         "NULL::bigint AS prorettype FROM pg_catalog.pg_proc p");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_index",
                         "SELECT i.indexrelid AS indexrelid, i.indrelid AS indrelid, "
                         "i.indkey AS indkey FROM pg_catalog.pg_index i");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_tablespace",
                         "SELECT t.oid AS oid, t.spcname AS spcname, "
                         "t.spcowner AS spcowner, t.spcacl AS spcacl, "
                         "t.spcoptions AS spcoptions FROM pg_catalog.pg_tablespace t");
    if (status != core::Status::OK) return status;

    return core::Status::OK;
}

core::Status PostgresqlAdapter::compileQuery(const std::string& sql,
                                             std::vector<uint8_t>& bytecode_out,
                                             std::string& error_out) {
    // Keep PostgreSQL compilation deterministic and rooted in the selected DB handle.
    sblr::PostgreSQLQueryCompiler compiler(nullptr);
    std::string selected_database;
    if (!resolveDatabaseSelection(database_name_, selected_database)) {
        if (config_.enforce_bound_database &&
            !config_.default_database.empty() &&
            isPostgresqlSystemDatabaseName(database_name_)) {
            selected_database = config_.default_database;
        } else {
            error_out = "Database switch denied by manager binding context";
            return core::Status::INVALID_AUTHORIZATION;
        }
    }
    std::string db_name = resolveEmulatedDatabaseForSession(client_parameters_, selected_database);
    if (db_name.empty()) {
        db_name = selected_database;
    }
    // Emulated PostgreSQL sessions are rooted at the selected logical database schema.
    std::string schema_root = resolvePostgresqlSchemaPath(
        engineDatabase() ? engineDatabase()->catalog_manager() : nullptr,
        db_name);
    compiler.setDefaultSchema(schema_root.empty() ? db_name : schema_root);
    if (pgWireDebugEnabled()) {
        std::fprintf(stderr,
                     "[pg_wire] compileQuery selected_db=%s emulated_db=%s schema_root=%s\n",
                     selected_database.c_str(),
                     db_name.c_str(),
                     schema_root.c_str());
        std::fflush(stderr);
    }
    auto result = compiler.compile(sql);
    if (!result.success()) {
        error_out = result.errors().empty() ? "Compilation failed" : result.errors().front();
        return core::Status::INVALID_ARGUMENT;
    }
    bytecode_out = result.bytecode();
    return core::Status::OK;
}

int16_t PostgresqlAdapter::selectFormatForColumn(size_t idx,
                                                 const std::vector<int16_t>& formats,
                                                 WireType type) {
    int16_t format = 0;
    if (!formats.empty()) {
        if (formats.size() == 1) {
            format = formats.front();
        } else if (idx < formats.size()) {
            format = formats[idx];
        }
    }
    if (format != 1) {
        format = 0;
    }
    if (format == 1 && !supportsBinaryFormat(type)) {
        format = 0;
    }
    return format;
}

bool PostgresqlAdapter::supportsBinaryFormat(WireType type) {
    switch (type) {
        case WireType::BOOLEAN:
        case WireType::INT16:
        case WireType::INT32:
        case WireType::INT64:
        case WireType::FLOAT32:
        case WireType::FLOAT64:
        case WireType::BYTEA:
        case WireType::VARCHAR:
        case WireType::CHAR:
        case WireType::JSON:
        case WireType::JSONB:
        case WireType::XML:
            return true;
        default:
            return false;
    }
}

static std::string hexEncode(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    oss << "\\x";
    oss << std::hex << std::setfill('0');
    for (uint8_t b : bytes) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::vector<uint8_t> PostgresqlAdapter::encodeTextValue(const ProtocolCodec::ColumnValue& val,
                                                        WireType type) {
    if (val.is_null) {
        return {};
    }

    auto toStringInt = [&](auto value) {
        return std::to_string(value);
    };

    std::string out;
    switch (type) {
        case WireType::BOOLEAN: {
            bool b = !val.data.empty() && val.data[0] != 0;
            out = b ? "t" : "f";
            break;
        }
        case WireType::INT16: {
            int16_t v = 0;
            if (val.data.size() >= sizeof(int16_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int16_t));
            }
            out = toStringInt(v);
            break;
        }
        case WireType::INT32: {
            int32_t v = 0;
            if (val.data.size() >= sizeof(int32_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int32_t));
            }
            out = toStringInt(v);
            break;
        }
        case WireType::INT64: {
            int64_t v = 0;
            if (val.data.size() >= sizeof(int64_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int64_t));
            }
            out = toStringInt(v);
            break;
        }
        case WireType::FLOAT32: {
            float v = 0.0f;
            if (val.data.size() >= sizeof(float)) {
                std::memcpy(&v, val.data.data(), sizeof(float));
            }
            out = toStringInt(static_cast<double>(v));
            break;
        }
        case WireType::FLOAT64: {
            double v = 0.0;
            if (val.data.size() >= sizeof(double)) {
                std::memcpy(&v, val.data.data(), sizeof(double));
            }
            out = toStringInt(v);
            break;
        }
        case WireType::BYTEA: {
            out = hexEncode(val.data);
            break;
        }
        default: {
            out.assign(reinterpret_cast<const char*>(val.data.data()),
                       val.data.size());
            break;
        }
    }

    return std::vector<uint8_t>(out.begin(), out.end());
}

template <typename T>
static void appendBigEndian(std::vector<uint8_t>& out, T value) {
    for (int i = sizeof(T) - 1; i >= 0; --i) {
        out.push_back(static_cast<uint8_t>((static_cast<uint64_t>(value) >> (i * 8)) & 0xFF));
    }
}

std::vector<uint8_t> PostgresqlAdapter::encodeBinaryValue(const ProtocolCodec::ColumnValue& val,
                                                          WireType type,
                                                          bool& ok) {
    ok = true;
    if (val.is_null) {
        return {};
    }
    std::vector<uint8_t> out;
    switch (type) {
        case WireType::BOOLEAN: {
            out.push_back(!val.data.empty() && val.data[0] != 0 ? 1 : 0);
            break;
        }
        case WireType::INT16: {
            int16_t v = 0;
            if (val.data.size() >= sizeof(int16_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int16_t));
            }
            appendBigEndian<int16_t>(out, v);
            break;
        }
        case WireType::INT32: {
            int32_t v = 0;
            if (val.data.size() >= sizeof(int32_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int32_t));
            }
            appendBigEndian<int32_t>(out, v);
            break;
        }
        case WireType::INT64: {
            int64_t v = 0;
            if (val.data.size() >= sizeof(int64_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int64_t));
            }
            appendBigEndian<int64_t>(out, v);
            break;
        }
        case WireType::FLOAT32: {
            uint32_t bits = 0;
            if (val.data.size() >= sizeof(uint32_t)) {
                std::memcpy(&bits, val.data.data(), sizeof(uint32_t));
            }
            appendBigEndian<uint32_t>(out, bits);
            break;
        }
        case WireType::FLOAT64: {
            uint64_t bits = 0;
            if (val.data.size() >= sizeof(uint64_t)) {
                std::memcpy(&bits, val.data.data(), sizeof(uint64_t));
            }
            appendBigEndian<uint64_t>(out, bits);
            break;
        }
        case WireType::BYTEA:
        case WireType::VARCHAR:
        case WireType::CHAR:
        case WireType::JSON:
        case WireType::JSONB:
        case WireType::XML: {
            out = val.data;
            break;
        }
        default: {
            ok = false;
            break;
        }
    }
    return out;
}

std::string PostgresqlAdapter::decodeBinaryParamToText(WireType type,
                                                       const uint8_t* data,
                                                       size_t len) {
    if (data == nullptr || len == 0) {
        return {};
    }
    auto readBE16 = [&](const uint8_t* p) -> int16_t {
        return static_cast<int16_t>((p[0] << 8) | p[1]);
    };
    auto readBE32 = [&](const uint8_t* p) -> int32_t {
        return (static_cast<int32_t>(p[0]) << 24) |
               (static_cast<int32_t>(p[1]) << 16) |
               (static_cast<int32_t>(p[2]) << 8) |
               static_cast<int32_t>(p[3]);
    };
    auto readBE64 = [&](const uint8_t* p) -> int64_t {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v = (v << 8) | p[i];
        }
        return static_cast<int64_t>(v);
    };

    switch (type) {
        case WireType::BOOLEAN:
            return data[0] ? "t" : "f";
        case WireType::INT16:
            if (len >= 2) return std::to_string(readBE16(data));
            break;
        case WireType::INT32:
            if (len >= 4) return std::to_string(readBE32(data));
            break;
        case WireType::INT64:
            if (len >= 8) return std::to_string(readBE64(data));
            break;
        case WireType::FLOAT32:
            if (len >= 4) {
                uint32_t bits = static_cast<uint32_t>(readBE32(data));
                float f;
                std::memcpy(&f, &bits, sizeof(float));
                return std::to_string(static_cast<double>(f));
            }
            break;
        case WireType::FLOAT64:
            if (len >= 8) {
                uint64_t bits = static_cast<uint64_t>(readBE64(data));
                double d;
                std::memcpy(&d, &bits, sizeof(double));
                return std::to_string(d);
            }
            break;
        case WireType::BYTEA:
            return hexEncode(std::vector<uint8_t>(data, data + len));
        default:
            break;
    }
    return std::string(reinterpret_cast<const char*>(data), len);
}

std::string PostgresqlAdapter::escapeLiteral(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input) {
        out.push_back(c);
        if (c == '\'') {
            out.push_back('\'');
        }
    }
    return out;
}

std::string PostgresqlAdapter::substitutePositionalParameters(const QueryContext& query) {
    if (query.parameter_values.empty()) {
        return query.query;
    }

    std::string sql = query.query;
    size_t param_count = query.parameter_values.size();

    auto literal_for_index = [&](size_t idx) -> std::string {
        if (idx >= query.parameter_values.size()) {
            return "NULL";
        }
        if (!query.parameter_nulls.empty() && query.parameter_nulls[idx]) {
            return "NULL";
        }
        return "'" + escapeLiteral(query.parameter_values[idx]) + "'";
    };

    // Scan and replace while skipping quoted literals and identifiers
    for (size_t i = param_count; i >= 1; --i) {
        std::string placeholder = "$" + std::to_string(i);
        std::string replacement = literal_for_index(i - 1);

        std::string rewritten;
        rewritten.reserve(sql.size());
        enum class ScanState { Normal, SingleQuote, DoubleQuote, DollarQuote };
        ScanState state = ScanState::Normal;
        std::string dollar_tag;

        for (size_t pos = 0; pos < sql.size();) {
            char c = sql[pos];
            if (state == ScanState::Normal) {
                if (c == '\'') {
                    state = ScanState::SingleQuote;
                    rewritten.push_back(c);
                    ++pos;
                    continue;
                }
                if (c == '"') {
                    state = ScanState::DoubleQuote;
                    rewritten.push_back(c);
                    ++pos;
                    continue;
                }
                if (c == '$') {
                    // Check for dollar-quote start
                    size_t tag_end = sql.find('$', pos + 1);
                    if (tag_end != std::string::npos && tag_end > pos + 1) {
                        dollar_tag = sql.substr(pos, tag_end - pos + 1);
                        state = ScanState::DollarQuote;
                        rewritten.append(dollar_tag);
                        pos = tag_end + 1;
                        continue;
                    }
                }
                // Placeholder replacement
                if (sql.compare(pos, placeholder.size(), placeholder) == 0) {
                    rewritten.append(replacement);
                    pos += placeholder.size();
                    continue;
                }
                rewritten.push_back(c);
                ++pos;
            } else if (state == ScanState::SingleQuote) {
                rewritten.push_back(c);
                ++pos;
                if (c == '\'' && !(pos < sql.size() && sql[pos] == '\'')) {
                    state = ScanState::Normal;
                } else if (c == '\'' && pos < sql.size() && sql[pos] == '\'') {
                    // Escaped single quote
                    rewritten.push_back(sql[pos]);
                    ++pos;
                }
            } else if (state == ScanState::DoubleQuote) {
                rewritten.push_back(c);
                ++pos;
                if (c == '"') {
                    state = ScanState::Normal;
                }
            } else if (state == ScanState::DollarQuote) {
                if (!dollar_tag.empty() && sql.compare(pos, dollar_tag.size(), dollar_tag) == 0) {
                    rewritten.append(dollar_tag);
                    pos += dollar_tag.size();
                    state = ScanState::Normal;
                } else {
                    rewritten.push_back(c);
                    ++pos;
                }
            }
        }
        sql.swap(rewritten);
        if (i == 1) break;  // prevent underflow
    }
    return sql;
}

bool PostgresqlAdapter::parseCopyQuery(const std::string& sql, CopyContext& ctx, std::string& error) {
    ctx = CopyContext{};
    error.clear();

    auto trim = [](const std::string& input) {
        size_t start = 0;
        while (start < input.size() &&
               std::isspace(static_cast<unsigned char>(input[start]))) {
            ++start;
        }
        size_t end = input.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(input[end - 1]))) {
            --end;
        }
        return input.substr(start, end - start);
    };

    auto to_upper = [](const std::string& input) {
        std::string out;
        out.reserve(input.size());
        for (char c : input) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        return out;
    };

    std::string input = trim(sql);
    if (input.empty()) {
        return false;
    }
    if (!input.empty() && input.back() == ';') {
        input.pop_back();
        input = trim(input);
    }
    if (input.size() < 4) {
        return false;
    }
    auto prefix = to_upper(input.substr(0, 4));
    if (prefix != "COPY") {
        return false;
    }
    if (input.size() > 4) {
        char next = input[4];
        if (!std::isspace(static_cast<unsigned char>(next)) && next != '(') {
            return false;
        }
    }

    size_t pos = 4;
    auto skip_ws = [&](size_t& p) {
        while (p < input.size() &&
               std::isspace(static_cast<unsigned char>(input[p]))) {
            ++p;
        }
    };
    skip_ws(pos);

    auto parse_quoted_identifier = [&](size_t& p, std::string& out) -> bool {
        if (p >= input.size() || input[p] != '"') {
            return false;
        }
        size_t start = p;
        ++p;
        std::string inner;
        while (p < input.size()) {
            char c = input[p];
            if (c == '"') {
                if (p + 1 < input.size() && input[p + 1] == '"') {
                    inner.push_back('"');
                    p += 2;
                    continue;
                }
                ++p;
                out = "\"" + inner + "\"";
                return true;
            }
            inner.push_back(c);
            ++p;
        }
        error = "COPY identifier missing closing quote";
        return false;
    };

    auto parse_unquoted_identifier = [&](size_t& p, std::string& out) -> bool {
        size_t start = p;
        while (p < input.size()) {
            char c = input[p];
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
                ++p;
                continue;
            }
            break;
        }
        if (start == p) {
            return false;
        }
        out = input.substr(start, p - start);
        return true;
    };

    auto parse_identifier_part = [&](size_t& p, std::string& out) -> bool {
        skip_ws(p);
        if (p >= input.size()) {
            return false;
        }
        if (input[p] == '"') {
            return parse_quoted_identifier(p, out);
        }
        return parse_unquoted_identifier(p, out);
    };

    auto parse_qualified_identifier = [&](size_t& p, std::string& out) -> bool {
        out.clear();
        std::string part;
        if (!parse_identifier_part(p, part)) {
            error = "COPY expected identifier";
            return false;
        }
        out = part;
        skip_ws(p);
        while (p < input.size() && input[p] == '.') {
            out.push_back('.');
            ++p;
            if (!parse_identifier_part(p, part)) {
                error = "COPY expected identifier after '.'";
                return false;
            }
            out += part;
            skip_ws(p);
        }
        return true;
    };

    if (pos < input.size() && input[pos] == '(') {
        size_t start = pos + 1;
        size_t depth = 1;
        bool in_single = false;
        bool in_double = false;
        ++pos;
        while (pos < input.size()) {
            char c = input[pos];
            if (in_single) {
                if (c == '\'') {
                    if (pos + 1 < input.size() && input[pos + 1] == '\'') {
                        pos += 2;
                        continue;
                    }
                    in_single = false;
                }
                ++pos;
                continue;
            }
            if (in_double) {
                if (c == '"') {
                    if (pos + 1 < input.size() && input[pos + 1] == '"') {
                        pos += 2;
                        continue;
                    }
                    in_double = false;
                }
                ++pos;
                continue;
            }
            if (c == '\'') {
                in_single = true;
                ++pos;
                continue;
            }
            if (c == '"') {
                in_double = true;
                ++pos;
                continue;
            }
            if (c == '(') {
                ++depth;
                ++pos;
                continue;
            }
            if (c == ')') {
                --depth;
                if (depth == 0) {
                    ctx.select_query = trim(input.substr(start, pos - start));
                    ++pos;
                    break;
                }
                ++pos;
                continue;
            }
            ++pos;
        }
        if (depth != 0) {
            error = "COPY missing ')' for SELECT";
            return true;
        }
        if (ctx.select_query.empty() ||
            to_upper(ctx.select_query.substr(0, std::min<size_t>(6, ctx.select_query.size()))) != "SELECT") {
            error = "COPY requires SELECT inside parentheses";
            return true;
        }
    } else {
        if (!parse_qualified_identifier(pos, ctx.table_name)) {
            if (error.empty()) {
                error = "COPY expected table name";
            }
            return true;
        }
        skip_ws(pos);
        if (pos < input.size() && input[pos] == '(') {
            ++pos;
            while (pos < input.size()) {
                skip_ws(pos);
                if (pos < input.size() && input[pos] == ')') {
                    ++pos;
                    break;
                }
                std::string col;
                if (!parse_identifier_part(pos, col)) {
                    error = "COPY expected column name";
                    return true;
                }
                ctx.columns.push_back(col);
                skip_ws(pos);
                if (pos < input.size() && input[pos] == ',') {
                    ++pos;
                    continue;
                }
                if (pos < input.size() && input[pos] == ')') {
                    ++pos;
                    break;
                }
                error = "COPY expected ',' or ')' in column list";
                return true;
            }
        }
    }

    skip_ws(pos);
    auto read_word = [&](size_t& p) -> std::string {
        skip_ws(p);
        size_t start = p;
        while (p < input.size()) {
            char c = input[p];
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                ++p;
                continue;
            }
            break;
        }
        return input.substr(start, p - start);
    };

    std::string direction = read_word(pos);
    std::string dir_upper = to_upper(direction);
    if (dir_upper != "FROM" && dir_upper != "TO") {
        error = "COPY requires FROM or TO";
        return true;
    }
    bool is_from = (dir_upper == "FROM");
    bool is_to = (dir_upper == "TO");

    auto parse_target = [&](size_t& p, std::string& out) -> bool {
        skip_ws(p);
        if (p >= input.size()) {
            return false;
        }
        if (input[p] == '\'') {
            ++p;
            std::string value;
            while (p < input.size()) {
                char c = input[p];
                if (c == '\'') {
                    if (p + 1 < input.size() && input[p + 1] == '\'') {
                        value.push_back('\'');
                        p += 2;
                        continue;
                    }
                    ++p;
                    out = value;
                    return true;
                }
                value.push_back(c);
                ++p;
            }
            error = "COPY target missing closing quote";
            return false;
        }
        std::string word = read_word(p);
        if (word.empty()) {
            return false;
        }
        out = word;
        return true;
    };

    std::string target;
    if (!parse_target(pos, target)) {
        if (error.empty()) {
            error = "COPY requires a target";
        }
        return true;
    }

    std::string target_upper = to_upper(target);
    if (is_from && target_upper == "STDIN") {
        ctx.from_stdin = true;
    } else if (is_to && target_upper == "STDOUT") {
        ctx.to_stdout = true;
    } else {
        // Non-stream COPY targets (e.g., COPY ... FROM '/path/file') are
        // executed directly by the engine. The adapter only intercepts
        // COPY STDIN/STDOUT for pgwire stream framing.
        return false;
    }

    struct Token {
        enum class Kind { Identifier, String, Symbol, End };
        Kind kind = Kind::End;
        std::string text;
        char symbol = 0;
    };
    struct Tokenizer {
        const std::string& input;
        size_t pos = 0;
        bool has_peek = false;
        Token peek_token;

        static bool is_ident_char(char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        Token next() {
            if (has_peek) {
                has_peek = false;
                return peek_token;
            }
            while (pos < input.size() &&
                   std::isspace(static_cast<unsigned char>(input[pos]))) {
                ++pos;
            }
            if (pos >= input.size()) {
                return {};
            }
            char c = input[pos];
            if (c == '(' || c == ')' || c == ',' || c == '=') {
                ++pos;
                Token tok;
                tok.kind = Token::Kind::Symbol;
                tok.symbol = c;
                return tok;
            }
            if (c == '\'') {
                ++pos;
                std::string value;
                while (pos < input.size()) {
                    char ch = input[pos];
                    if (ch == '\'') {
                        if (pos + 1 < input.size() && input[pos + 1] == '\'') {
                            value.push_back('\'');
                            pos += 2;
                            continue;
                        }
                        ++pos;
                        Token tok;
                        tok.kind = Token::Kind::String;
                        tok.text = value;
                        return tok;
                    }
                    value.push_back(ch);
                    ++pos;
                }
                Token tok;
                tok.kind = Token::Kind::String;
                tok.text = value;
                return tok;
            }
            if (c == '"') {
                ++pos;
                std::string value;
                while (pos < input.size()) {
                    char ch = input[pos];
                    if (ch == '"') {
                        if (pos + 1 < input.size() && input[pos + 1] == '"') {
                            value.push_back('"');
                            pos += 2;
                            continue;
                        }
                        ++pos;
                        Token tok;
                        tok.kind = Token::Kind::Identifier;
                        tok.text = value;
                        return tok;
                    }
                    value.push_back(ch);
                    ++pos;
                }
                Token tok;
                tok.kind = Token::Kind::Identifier;
                tok.text = value;
                return tok;
            }
            size_t start = pos;
            while (pos < input.size() && is_ident_char(input[pos])) {
                ++pos;
            }
            Token tok;
            tok.kind = Token::Kind::Identifier;
            tok.text = input.substr(start, pos - start);
            return tok;
        }

        Token peek() {
            if (!has_peek) {
                peek_token = next();
                has_peek = true;
            }
            return peek_token;
        }
    };

    auto decode_escape = [](const std::string& value) {
        if (value.size() == 2 && value[0] == '\\') {
            switch (value[1]) {
                case 't': return std::string(1, '\t');
                case 'n': return std::string(1, '\n');
                case 'r': return std::string(1, '\r');
                case '\\': return std::string(1, '\\');
                default: break;
            }
        }
        return value;
    };

    bool delimiter_set = false;
    bool null_set = false;

    auto parse_options = [&](const std::string& options) -> bool {
        std::string opt_text = trim(options);
        if (opt_text.empty()) {
            return true;
        }
        Tokenizer tok{opt_text};
        Token t = tok.next();
        if (t.kind == Token::Kind::Identifier &&
            to_upper(t.text) == "WITH") {
            t = tok.next();
        }

        bool in_paren = false;
        if (t.kind == Token::Kind::Symbol && t.symbol == '(') {
            in_paren = true;
            t = tok.next();
        }

        auto consume_value = [&]() -> Token {
            Token val = tok.next();
            if (val.kind == Token::Kind::Symbol && val.symbol == '=') {
                val = tok.next();
            }
            return val;
        };

        while (t.kind != Token::Kind::End) {
            if (t.kind == Token::Kind::Symbol && t.symbol == ')') {
                if (!in_paren) {
                    error = "COPY unexpected ')'";
                    return false;
                }
                break;
            }
            if (t.kind != Token::Kind::Identifier) {
                error = "COPY expected option name";
                return false;
            }
            std::string opt = to_upper(t.text);

            if (opt == "FORMAT") {
                Token fmt = consume_value();
                if (fmt.kind != Token::Kind::Identifier && fmt.kind != Token::Kind::String) {
                    error = "COPY FORMAT requires a value";
                    return false;
                }
                std::string fmt_upper = to_upper(fmt.text);
                if (fmt_upper == "CSV") {
                    ctx.options.format = CopyOptions::Format::CSV;
                } else if (fmt_upper == "TEXT") {
                    ctx.options.format = CopyOptions::Format::TEXT;
                } else {
                    error = "COPY FORMAT supports CSV or TEXT only";
                    return false;
                }
            } else if (opt == "CSV" || opt == "TEXT") {
                ctx.options.format = (opt == "CSV")
                    ? CopyOptions::Format::CSV
                    : CopyOptions::Format::TEXT;
            } else if (opt == "DELIMITER") {
                Token delim = consume_value();
                if (delim.kind != Token::Kind::Identifier && delim.kind != Token::Kind::String) {
                    error = "COPY DELIMITER requires a value";
                    return false;
                }
                std::string decoded = decode_escape(delim.text);
                if (decoded.size() != 1) {
                    error = "COPY DELIMITER must be a single character";
                    return false;
                }
                ctx.options.delimiter = decoded[0];
                delimiter_set = true;
            } else if (opt == "NULL") {
                Token null_tok = consume_value();
                if (null_tok.kind != Token::Kind::Identifier && null_tok.kind != Token::Kind::String) {
                    error = "COPY NULL requires a value";
                    return false;
                }
                ctx.options.null_string = null_tok.text;
                null_set = true;
            } else if (opt == "HEADER") {
                Token next_tok = tok.peek();
                if (next_tok.kind == Token::Kind::Symbol && next_tok.symbol == '=') {
                    Token flag = consume_value();
                    std::string flag_upper = to_upper(flag.text);
                    if (flag_upper == "TRUE" || flag_upper == "ON" || flag_upper == "1") {
                        ctx.options.header = true;
                    } else if (flag_upper == "FALSE" || flag_upper == "OFF" || flag_upper == "0") {
                        ctx.options.header = false;
                    } else {
                        ctx.options.header = true;
                    }
                } else if (next_tok.kind == Token::Kind::Identifier || next_tok.kind == Token::Kind::String) {
                    Token flag = consume_value();
                    std::string flag_upper = to_upper(flag.text);
                    if (flag_upper == "TRUE" || flag_upper == "ON" || flag_upper == "1") {
                        ctx.options.header = true;
                    } else if (flag_upper == "FALSE" || flag_upper == "OFF" || flag_upper == "0") {
                        ctx.options.header = false;
                    } else {
                        ctx.options.header = true;
                    }
                } else {
                    ctx.options.header = true;
                }
            } else {
                error = "COPY option not supported: " + opt;
                return false;
            }

            t = tok.next();
            if (t.kind == Token::Kind::Symbol && t.symbol == ',') {
                t = tok.next();
            }
        }

        if (in_paren) {
            if (t.kind != Token::Kind::Symbol || t.symbol != ')') {
                error = "COPY options missing ')'";
                return false;
            }
            t = tok.next();
        }
        if (t.kind != Token::Kind::End) {
            error = "COPY unexpected trailing options";
            return false;
        }
        return true;
    };

    if (pos < input.size()) {
        std::string options = trim(input.substr(pos));
        if (!options.empty()) {
            if (!parse_options(options)) {
                return true;
            }
        }
    }

    if (ctx.options.format == CopyOptions::Format::CSV) {
        if (!delimiter_set) {
            ctx.options.delimiter = ',';
        }
        if (!null_set) {
            ctx.options.null_string.clear();
        }
    }

    return true;
}

core::Status PostgresqlAdapter::startCopyOut(network::Connection* conn, CopyContext& ctx) {
    if (ctx.table_name.empty() && ctx.select_query.empty()) {
        sendErrorResponse(conn, "ERROR", "0A000", "COPY TO STDOUT requires a table or SELECT");
        if (!ctx.from_extended) {
            sendReadyForQuery(conn);
        }
        return sendBuffer(conn);
    }

    copy_context_ = ctx;
    copy_context_.active = true;
    copy_context_.bytes = 0;
    copy_context_.rows = 0;
    copy_context_.start_time = std::chrono::steady_clock::now();
    pg_state_ = PgProtocolState::COPY_OUT;

    QueryContext query;
    if (!ctx.select_query.empty()) {
        query.query = ctx.select_query;
    } else {
        std::ostringstream sql;
        sql << "SELECT ";
        if (ctx.columns.empty()) {
            sql << "*";
        } else {
            for (size_t i = 0; i < ctx.columns.size(); ++i) {
                if (i > 0) sql << ", ";
                sql << ctx.columns[i];
            }
        }
        sql << " FROM " << ctx.table_name;
        query.query = sql.str();
    }

    ResultContext result;
    executeRemoteQuery(query, result);
    if (result.has_error) {
        sendProtocolError(conn, result.error_code, result.sqlstate,
                          result.error_message, result.error_detail, result.error_hint);
        recordCopyMetrics("to", 0, 0, true, copy_context_.start_time);
        copy_context_ = CopyContext{};
        pg_state_ = PgProtocolState::READY;
        if (!ctx.from_extended) {
            sendReadyForQuery(conn);
        }
        return sendBuffer(conn);
    }

    sendCopyOutResponse(conn, 0, {});

    auto escape_text_field = [&](const std::string& value) {
        std::string out;
        out.reserve(value.size() + 4);
        for (char c : value) {
            switch (c) {
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\\': out += "\\\\"; break;
                default:
                    if (c == ctx.options.delimiter) {
                        out.push_back('\\');
                    }
                    out.push_back(c);
                    break;
            }
        }
        return out;
    };

    auto escape_csv_field = [&](const std::string& value) {
        bool needs_quotes = value.empty() && ctx.options.null_string.empty();
        for (char c : value) {
            if (c == ctx.options.delimiter || c == '"' || c == '\n' || c == '\r') {
                needs_quotes = true;
                break;
            }
        }
        if (!needs_quotes) {
            return value;
        }
        std::string out = "\"";
        for (char c : value) {
            if (c == '"') {
                out += "\"\"";
            } else {
                out.push_back(c);
            }
        }
        out.push_back('"');
        return out;
    };

    auto format_field = [&](const std::string& value, bool is_null) {
        if (is_null) {
            return ctx.options.null_string;
        }
        if (ctx.options.format == CopyOptions::Format::CSV) {
            return escape_csv_field(value);
        }
        return escape_text_field(value);
    };

    uint64_t bytes_sent = 0;
    auto send_row = [&](const std::vector<std::string>& fields) {
        std::string line;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) {
                line.push_back(ctx.options.delimiter);
            }
            line += fields[i];
        }
        line.push_back('\n');
        bytes_sent += line.size();
        sendCopyData(conn, line.data(), line.size());
    };

    if (ctx.options.header) {
        std::vector<std::string> header_fields;
        header_fields.reserve(result.columns.size());
        for (const auto& col : result.columns) {
            header_fields.push_back(format_field(col.name, false));
        }
        send_row(header_fields);
    }

    for (const auto& row : result.rows) {
        std::vector<std::string> fields;
        fields.reserve(result.columns.size());
        for (size_t i = 0; i < result.columns.size(); ++i) {
            const auto& col = result.columns[i];
            const auto& val = row[i];
            std::string text;
            if (!val.is_null) {
                auto encoded = encodeTextValue(val, col.type);
                text.assign(reinterpret_cast<const char*>(encoded.data()), encoded.size());
            }
            fields.push_back(format_field(text, val.is_null));
        }
        send_row(fields);
    }

    sendCopyDone(conn);

    copy_context_.rows = result.rows.size();
    copy_context_.bytes = bytes_sent;
    std::string tag = "COPY " + std::to_string(copy_context_.rows);
    sendCommandComplete(conn, tag);

    recordCopyMetrics("to", copy_context_.rows, copy_context_.bytes, false,
                      copy_context_.start_time);

    if (ctx.from_extended && !ctx.portal_name.empty()) {
        auto it = portals_.find(ctx.portal_name);
        if (it != portals_.end()) {
            it->second.executed = true;
            it->second.completed = true;
            it->second.command_tag = tag;
        }
    }

    copy_context_ = CopyContext{};
    pg_state_ = PgProtocolState::READY;
    if (!ctx.from_extended) {
        sendReadyForQuery(conn);
    }
    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::startCopyIn(network::Connection* conn, CopyContext& ctx) {
    if (!ctx.select_query.empty()) {
        sendErrorResponse(conn, "ERROR", "0A000", "COPY FROM STDIN does not accept SELECT");
        if (!ctx.from_extended) {
            sendReadyForQuery(conn);
        }
        return sendBuffer(conn);
    }
    if (ctx.table_name.empty()) {
        sendErrorResponse(conn, "ERROR", "0A000", "COPY FROM STDIN requires a table");
        if (!ctx.from_extended) {
            sendReadyForQuery(conn);
        }
        return sendBuffer(conn);
    }

    copy_context_ = ctx;
    copy_context_.active = true;
    copy_context_.buffer.clear();
    copy_context_.rows = 0;
    copy_context_.bytes = 0;
    copy_context_.start_time = std::chrono::steady_clock::now();
    pg_state_ = PgProtocolState::COPY_IN;

    sendCopyInResponse(conn, 0, {});

    if (ctx.from_extended && !ctx.portal_name.empty()) {
        auto it = portals_.find(ctx.portal_name);
        if (it != portals_.end()) {
            it->second.executed = true;
            it->second.completed = false;
        }
    }

    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::finishCopyIn(network::Connection* conn) {
    CopyContext ctx = copy_context_;

    struct ParsedValue {
        bool is_null = false;
        std::string value;
        bool quoted = false;
    };

    auto parse_text = [&](const std::string& data,
                          std::vector<std::vector<ParsedValue>>& rows,
                          std::string& err) -> bool {
        (void)err;
        std::vector<ParsedValue> row;
        std::string raw;
        std::string decoded;
        bool escaping = false;

        auto finish_field = [&]() {
            ParsedValue field;
            field.is_null = (raw == ctx.options.null_string);
            if (!field.is_null) {
                field.value = decoded;
            }
            row.push_back(field);
            raw.clear();
            decoded.clear();
        };

        auto finish_row = [&]() {
            finish_field();
            rows.push_back(row);
            row.clear();
        };

        for (size_t i = 0; i < data.size(); ++i) {
            char c = data[i];
            if (!escaping && c == ctx.options.delimiter) {
                finish_field();
                continue;
            }
            if (!escaping && (c == '\n' || c == '\r')) {
                finish_row();
                if (c == '\r' && i + 1 < data.size() && data[i + 1] == '\n') {
                    ++i;
                }
                continue;
            }
            if (!escaping && c == '\\') {
                raw.push_back(c);
                escaping = true;
                continue;
            }

            if (escaping) {
                raw.push_back(c);
                switch (c) {
                    case 'n': decoded.push_back('\n'); break;
                    case 'r': decoded.push_back('\r'); break;
                    case 't': decoded.push_back('\t'); break;
                    case '\\': decoded.push_back('\\'); break;
                    default: decoded.push_back(c); break;
                }
                escaping = false;
                continue;
            }

            raw.push_back(c);
            decoded.push_back(c);
        }

        if (!raw.empty() || !row.empty()) {
            finish_row();
        }
        return true;
    };

    auto parse_csv = [&](const std::string& data,
                         std::vector<std::vector<ParsedValue>>& rows,
                         std::string& err) -> bool {
        std::vector<ParsedValue> row;
        std::string field;
        bool in_quotes = false;
        bool quoted = false;

        auto finish_field = [&]() {
            ParsedValue value;
            value.quoted = quoted;
            value.is_null = (!quoted && field == ctx.options.null_string);
            if (!value.is_null) {
                value.value = field;
            }
            row.push_back(value);
            field.clear();
            quoted = false;
        };

        auto finish_row = [&]() {
            finish_field();
            rows.push_back(row);
            row.clear();
        };

        for (size_t i = 0; i < data.size(); ++i) {
            char c = data[i];
            if (in_quotes) {
                if (c == '"') {
                    if (i + 1 < data.size() && data[i + 1] == '"') {
                        field.push_back('"');
                        ++i;
                        continue;
                    }
                    in_quotes = false;
                    continue;
                }
                field.push_back(c);
                continue;
            }

            if (c == '"') {
                if (field.empty()) {
                    in_quotes = true;
                    quoted = true;
                    continue;
                }
                field.push_back(c);
                continue;
            }

            if (c == ctx.options.delimiter) {
                finish_field();
                continue;
            }

            if (c == '\n' || c == '\r') {
                finish_row();
                if (c == '\r' && i + 1 < data.size() && data[i + 1] == '\n') {
                    ++i;
                }
                continue;
            }

            field.push_back(c);
        }

        if (in_quotes) {
            err = "COPY CSV missing closing quote";
            return false;
        }

        if (!field.empty() || !row.empty()) {
            finish_row();
        }
        return true;
    };

    std::vector<std::vector<ParsedValue>> rows;
    std::string parse_error;
    bool ok = false;
    if (ctx.options.format == CopyOptions::Format::CSV) {
        ok = parse_csv(ctx.buffer, rows, parse_error);
    } else {
        ok = parse_text(ctx.buffer, rows, parse_error);
    }

    if (!ok) {
        sendErrorResponse(conn, "ERROR", "0A000",
                          parse_error.empty() ? "COPY parsing failed" : parse_error);
        recordCopyMetrics("from", 0, copy_context_.bytes, true, copy_context_.start_time);
        copy_context_ = CopyContext{};
        pg_state_ = PgProtocolState::READY;
        if (!ctx.from_extended) {
            sendReadyForQuery(conn);
        }
        return sendBuffer(conn);
    }

    size_t start_row = 0;
    if (ctx.options.header && !rows.empty()) {
        if (ctx.columns.empty()) {
            std::vector<std::string> header_cols;
            for (const auto& field : rows.front()) {
                std::string trimmed = field.value;
                size_t start = 0;
                while (start < trimmed.size() &&
                       std::isspace(static_cast<unsigned char>(trimmed[start]))) {
                    ++start;
                }
                size_t end = trimmed.size();
                while (end > start &&
                       std::isspace(static_cast<unsigned char>(trimmed[end - 1]))) {
                    --end;
                }
                std::string name = trimmed.substr(start, end - start);
                std::string quoted = "\"";
                for (char c : name) {
                    if (c == '"') {
                        quoted += "\"\"";
                    } else {
                        quoted.push_back(c);
                    }
                }
                quoted.push_back('"');
                header_cols.push_back(quoted);
            }
            ctx.columns = std::move(header_cols);
        }
        start_row = 1;
    }

    auto join_columns = [&](const std::vector<std::string>& cols) {
        std::ostringstream out;
        for (size_t i = 0; i < cols.size(); ++i) {
            if (i > 0) out << ", ";
            out << cols[i];
        }
        return out.str();
    };

    int64_t inserted = 0;
    for (size_t row_idx = start_row; row_idx < rows.size(); ++row_idx) {
        const auto& row = rows[row_idx];
        if (!ctx.columns.empty() && row.size() != ctx.columns.size()) {
            sendErrorResponse(conn, "ERROR", "0A000", "COPY column count does not match");
            recordCopyMetrics("from", inserted, copy_context_.bytes, true, copy_context_.start_time);
            copy_context_ = CopyContext{};
            pg_state_ = PgProtocolState::READY;
            if (!ctx.from_extended) {
                sendReadyForQuery(conn);
            }
            return sendBuffer(conn);
        }

        std::ostringstream sql;
        sql << "INSERT INTO " << ctx.table_name;
        if (!ctx.columns.empty()) {
            sql << " (" << join_columns(ctx.columns) << ")";
        }
        sql << " VALUES (";
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) sql << ", ";
            if (row[i].is_null) {
                sql << "NULL";
            } else {
                sql << "'" << escapeLiteral(row[i].value) << "'";
            }
        }
        sql << ")";

        QueryContext insert_ctx;
        insert_ctx.query = sql.str();
        ResultContext result;
        executeRemoteQuery(insert_ctx, result);
        if (result.has_error) {
            sendProtocolError(conn, result.error_code, result.sqlstate,
                              result.error_message, result.error_detail, result.error_hint);
            recordCopyMetrics("from", inserted, copy_context_.bytes, true, copy_context_.start_time);
            copy_context_ = CopyContext{};
            pg_state_ = PgProtocolState::READY;
            if (!ctx.from_extended) {
                sendReadyForQuery(conn);
            }
            return sendBuffer(conn);
        }
        inserted++;
    }

    std::string tag = "COPY " + std::to_string(inserted);
    sendCommandComplete(conn, tag);

    recordCopyMetrics("from", inserted, copy_context_.bytes, false, copy_context_.start_time);

    if (ctx.from_extended && !ctx.portal_name.empty()) {
        auto it = portals_.find(ctx.portal_name);
        if (it != portals_.end()) {
            it->second.executed = true;
            it->second.completed = true;
            it->second.command_tag = tag;
        }
    }

    copy_context_ = CopyContext{};
    pg_state_ = PgProtocolState::READY;
    if (!ctx.from_extended) {
        sendReadyForQuery(conn);
    }
    return sendBuffer(conn);
}

void PostgresqlAdapter::sendRowDescription(network::Connection* conn,
                                            const std::vector<ProtocolCodec::ColumnInfo>& columns,
                                            const std::vector<int16_t>& formats) {
    std::vector<uint8_t> payload;

    // Number of fields
    writeInt16(payload, static_cast<int16_t>(columns.size()));

    for (size_t i = 0; i < columns.size(); ++i) {
        const auto& col = columns[i];
        // Field name
        writeString(payload, col.name);

        // Table OID (0 = not from a table)
        writeInt32(payload, 0);

        // Column number (0 = not from a table)
        writeInt16(payload, 0);

        // Type OID
        writeInt32(payload, wireTypeToOid(col.type));

        // Type size
        writeInt16(payload, -1);  // Variable

        // Type modifier
        writeInt32(payload, static_cast<int32_t>(col.type_modifier));

        // Format code (0 = text, 1 = binary)
        writeInt16(payload, selectFormatForColumn(i, formats, col.type));
    }

    sendMessage(conn, pg::BackendMsg::ROW_DESCRIPTION, payload);
}

void PostgresqlAdapter::sendDataRow(network::Connection* conn,
                                     const std::vector<ProtocolCodec::ColumnInfo>& columns,
                                     const std::vector<ProtocolCodec::ColumnValue>& values,
                                     const std::vector<int16_t>& formats) {
    std::vector<uint8_t> payload;

    // Number of columns
    writeInt16(payload, static_cast<int16_t>(values.size()));

    for (size_t i = 0; i < values.size(); ++i) {
        const auto& val = values[i];
        WireType type = i < columns.size() ? columns[i].type : WireType::UNKNOWN;
        int16_t format = selectFormatForColumn(i, formats, type);

        if (val.is_null) {
            // NULL value
            writeInt32(payload, -1);
        } else {
            std::vector<uint8_t> encoded;
            if (format == 1) {
                bool ok = false;
                encoded = encodeBinaryValue(val, type, ok);
                if (!ok) {
                    // Fallback to text if binary not supported for this type/value
                    encoded = encodeTextValue(val, type);
                    format = 0;
                }
            } else {
                encoded = encodeTextValue(val, type);
            }
            writeInt32(payload, static_cast<int32_t>(encoded.size()));
            if (!encoded.empty()) {
                writeBytes(payload, encoded.data(), encoded.size());
            }
        }
    }

    sendMessage(conn, pg::BackendMsg::DATA_ROW, payload);
}

void PostgresqlAdapter::sendCommandComplete(network::Connection* conn, const std::string& tag) {
    std::vector<uint8_t> payload;
    writeString(payload, tag);
    sendMessage(conn, pg::BackendMsg::COMMAND_COMPLETE, payload);
}

void PostgresqlAdapter::sendEmptyQueryResponse(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::EMPTY_QUERY_RESPONSE, {});
}

void PostgresqlAdapter::sendParseComplete(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::PARSE_COMPLETE, {});
}

void PostgresqlAdapter::sendBindComplete(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::BIND_COMPLETE, {});
}

void PostgresqlAdapter::sendCloseComplete(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::CLOSE_COMPLETE, {});
}

void PostgresqlAdapter::sendNoData(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::NO_DATA, {});
}

void PostgresqlAdapter::sendParameterDescription(network::Connection* conn,
                                                  const std::vector<int32_t>& param_types) {
    std::vector<uint8_t> payload;

    writeInt16(payload, static_cast<int16_t>(param_types.size()));
    for (int32_t oid : param_types) {
        writeInt32(payload, oid);
    }

    sendMessage(conn, pg::BackendMsg::PARAMETER_DESCRIPTION, payload);
}

void PostgresqlAdapter::sendPortalSuspended(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::PORTAL_SUSPENDED, {});
}

void PostgresqlAdapter::sendErrorResponse(network::Connection* conn,
                                           const std::string& severity,
                                           const std::string& sqlstate,
                                           const std::string& message,
                                           const std::string& detail,
                                           const std::string& hint) {
    std::vector<uint8_t> payload;

    // Severity
    writeByte(payload, static_cast<uint8_t>(pg::ErrorField::SEVERITY));
    writeString(payload, severity);

    // SQLSTATE
    writeByte(payload, static_cast<uint8_t>(pg::ErrorField::CODE));
    writeString(payload, sqlstate.empty() ? "42000" : sqlstate);

    // Message
    writeByte(payload, static_cast<uint8_t>(pg::ErrorField::MESSAGE));
    writeString(payload, message);

    // Detail (optional)
    if (!detail.empty()) {
        writeByte(payload, static_cast<uint8_t>(pg::ErrorField::DETAIL));
        writeString(payload, detail);
    }

    // Hint (optional)
    if (!hint.empty()) {
        writeByte(payload, static_cast<uint8_t>(pg::ErrorField::HINT));
        writeString(payload, hint);
    }

    // Terminator
    writeByte(payload, 0);

    sendMessage(conn, pg::BackendMsg::ERROR_RESPONSE, payload);
}

void PostgresqlAdapter::sendNoticeResponse(network::Connection* conn,
                                            const std::string& severity,
                                            const std::string& message) {
    std::vector<uint8_t> payload;

    writeByte(payload, static_cast<uint8_t>(pg::ErrorField::SEVERITY));
    writeString(payload, severity);

    writeByte(payload, static_cast<uint8_t>(pg::ErrorField::MESSAGE));
    writeString(payload, message);

    writeByte(payload, 0);  // Terminator

    sendMessage(conn, pg::BackendMsg::NOTICE_RESPONSE, payload);
}

void PostgresqlAdapter::sendCopyInResponse(network::Connection* conn,
                                            int8_t overall_format,
                                            const std::vector<int16_t>& column_formats) {
    std::vector<uint8_t> payload;

    writeByte(payload, static_cast<uint8_t>(overall_format));
    writeInt16(payload, static_cast<int16_t>(column_formats.size()));
    for (int16_t fmt : column_formats) {
        writeInt16(payload, fmt);
    }

    sendMessage(conn, pg::BackendMsg::COPY_IN_RESPONSE, payload);
}

void PostgresqlAdapter::sendCopyOutResponse(network::Connection* conn,
                                             int8_t overall_format,
                                             const std::vector<int16_t>& column_formats) {
    std::vector<uint8_t> payload;

    writeByte(payload, static_cast<uint8_t>(overall_format));
    writeInt16(payload, static_cast<int16_t>(column_formats.size()));
    for (int16_t fmt : column_formats) {
        writeInt16(payload, fmt);
    }

    sendMessage(conn, pg::BackendMsg::COPY_OUT_RESPONSE, payload);
}

void PostgresqlAdapter::sendCopyData(network::Connection* conn, const void* data, size_t len) {
    std::vector<uint8_t> payload(static_cast<const uint8_t*>(data),
                                  static_cast<const uint8_t*>(data) + len);
    sendMessage(conn, pg::BackendMsg::COPY_DATA, payload);
}

void PostgresqlAdapter::sendCopyDone(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::COPY_DONE, {});
}

// ============================================================================
// Helper Methods
// ============================================================================

void PostgresqlAdapter::writeInt32(std::vector<uint8_t>& buf, int32_t value) {
    // Network byte order (big-endian)
    buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

void PostgresqlAdapter::writeInt16(std::vector<uint8_t>& buf, int16_t value) {
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

void PostgresqlAdapter::writeByte(std::vector<uint8_t>& buf, uint8_t value) {
    buf.push_back(value);
}

void PostgresqlAdapter::writeString(std::vector<uint8_t>& buf, const std::string& str) {
    buf.insert(buf.end(), str.begin(), str.end());
    buf.push_back(0);  // Null terminator
}

void PostgresqlAdapter::writeBytes(std::vector<uint8_t>& buf, const void* data, size_t len) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    buf.insert(buf.end(), ptr, ptr + len);
}

int32_t PostgresqlAdapter::readInt32(const uint8_t* data) {
    return (static_cast<int32_t>(data[0]) << 24) |
           (static_cast<int32_t>(data[1]) << 16) |
           (static_cast<int32_t>(data[2]) << 8) |
           static_cast<int32_t>(data[3]);
}

int16_t PostgresqlAdapter::readInt16(const uint8_t* data) {
    return static_cast<int16_t>((static_cast<int16_t>(data[0]) << 8) |
                                 static_cast<int16_t>(data[1]));
}

std::string PostgresqlAdapter::readString(const uint8_t* data, size_t max_len) {
    std::string result;
    for (size_t i = 0; i < max_len && data[i] != 0; ++i) {
        result.push_back(static_cast<char>(data[i]));
    }
    return result;
}

int32_t PostgresqlAdapter::wireTypeToOid(WireType type) {
    switch (type) {
        case WireType::BOOLEAN: return pg::TypeOid::BOOL;
        case WireType::INT16: return pg::TypeOid::INT2;
        case WireType::INT32: return pg::TypeOid::INT4;
        case WireType::INT64: return pg::TypeOid::INT8;
        case WireType::FLOAT32: return pg::TypeOid::FLOAT4;
        case WireType::FLOAT64: return pg::TypeOid::FLOAT8;
        case WireType::DECIMAL: return pg::TypeOid::NUMERIC;
        case WireType::VARCHAR: return pg::TypeOid::VARCHAR;
        case WireType::CHAR: return pg::TypeOid::CHAR;
        case WireType::BYTEA: return pg::TypeOid::BYTEA;
        case WireType::DATE: return pg::TypeOid::DATE;
        case WireType::TIME: return pg::TypeOid::TIME;
        case WireType::TIMESTAMP: return pg::TypeOid::TIMESTAMP;
        case WireType::TIMESTAMPTZ: return pg::TypeOid::TIMESTAMPTZ;
        case WireType::INTERVAL: return pg::TypeOid::INTERVAL;
        case WireType::UUID: return pg::TypeOid::UUID;
        case WireType::JSON: return pg::TypeOid::JSON;
        case WireType::JSONB: return pg::TypeOid::JSONB;
        default: return pg::TypeOid::TEXT;
    }
}

WireType PostgresqlAdapter::oidToWireType(int32_t oid) {
    switch (oid) {
        case pg::TypeOid::BOOL: return WireType::BOOLEAN;
        case pg::TypeOid::INT2: return WireType::INT16;
        case pg::TypeOid::INT4: return WireType::INT32;
        case pg::TypeOid::INT8: return WireType::INT64;
        case pg::TypeOid::FLOAT4: return WireType::FLOAT32;
        case pg::TypeOid::FLOAT8: return WireType::FLOAT64;
        case pg::TypeOid::NUMERIC: return WireType::DECIMAL;
        case pg::TypeOid::VARCHAR: return WireType::VARCHAR;
        case pg::TypeOid::CHAR: return WireType::CHAR;
        case pg::TypeOid::TEXT: return WireType::VARCHAR;
        case pg::TypeOid::BYTEA: return WireType::BYTEA;
        case pg::TypeOid::DATE: return WireType::DATE;
        case pg::TypeOid::TIME: return WireType::TIME;
        case pg::TypeOid::TIMESTAMP: return WireType::TIMESTAMP;
        case pg::TypeOid::TIMESTAMPTZ: return WireType::TIMESTAMPTZ;
        case pg::TypeOid::INTERVAL: return WireType::INTERVAL;
        case pg::TypeOid::UUID: return WireType::UUID;
        case pg::TypeOid::JSON: return WireType::JSON;
        case pg::TypeOid::JSONB: return WireType::JSONB;
        default: return WireType::VARCHAR;
    }
}

void PostgresqlAdapter::mapStatusToSqlstate(uint32_t status, std::string& sqlstate) {
    switch (static_cast<core::Status>(status)) {
        case core::Status::SYNTAX_ERROR:
        case core::Status::INVALID_ARGUMENT:
            sqlstate = "42601";
            break;
        case core::Status::UNDEFINED_TABLE:
        case core::Status::NOT_FOUND:
            sqlstate = "42P01";
            break;
        case core::Status::UNDEFINED_COLUMN:
            sqlstate = "42703";
            break;
        case core::Status::CONSTRAINT_VIOLATION:
        case core::Status::UNIQUE_VIOLATION:
            sqlstate = "23505";
            break;
        case core::Status::FOREIGN_KEY_VIOLATION:
            sqlstate = "23503";
            break;
        case core::Status::CHECK_VIOLATION:
            sqlstate = "23514";
            break;
        case core::Status::NOT_NULL_VIOLATION:
        case core::Status::NULL_VALUE_NOT_ALLOWED:
            sqlstate = "23502";
            break;
        case core::Status::PERMISSION_DENIED:
        case core::Status::INSUFFICIENT_PRIVILEGE:
            sqlstate = "42501";
            break;
        case core::Status::INVALID_CURSOR_NAME:
        case core::Status::CURSOR_NOT_FOUND:
            sqlstate = "34000";
            break;
        case core::Status::INVALID_CURSOR_STATE:
        case core::Status::CURSOR_NOT_OPEN:
        case core::Status::CURSOR_ALREADY_OPEN:
            sqlstate = "24000";
            break;
        case core::Status::READ_ONLY_TRANSACTION:
            sqlstate = "25006";
            break;
        case core::Status::INVALID_TRANSACTION_STATE:
        case core::Status::NO_ACTIVE_TRANSACTION:
            sqlstate = "25P01";
            break;
        case core::Status::DEADLOCK:
            sqlstate = "40P01";
            break;
        case core::Status::SERIALIZATION_FAILURE:
            sqlstate = "40001";
            break;
        case core::Status::LOCK_TIMEOUT:
        case core::Status::LOCK_NOT_AVAILABLE:
            sqlstate = "55P03";
            break;
        case core::Status::QUERY_CANCELED:
            sqlstate = "57014";
            break;
        case core::Status::TOO_MANY_CONNECTIONS:
            sqlstate = "53300";
            break;
        case core::Status::NUMERIC_VALUE_OUT_OF_RANGE:
        case core::Status::OUT_OF_RANGE:
            sqlstate = "22003";
            break;
        case core::Status::STRING_DATA_RIGHT_TRUNCATION:
            sqlstate = "22001";
            break;
        case core::Status::DIVISION_BY_ZERO:
            sqlstate = "22012";
            break;
        case core::Status::INVALID_TEXT_REPRESENTATION:
            sqlstate = "22P02";
            break;
        case core::Status::DATETIME_FIELD_OVERFLOW:
            sqlstate = "22008";
            break;
        case core::Status::INVALID_DATETIME_FORMAT:
            sqlstate = "22007";
            break;
        case core::Status::DATATYPE_MISMATCH:
            sqlstate = "42804";
            break;
        case core::Status::NOT_IMPLEMENTED:
        case core::Status::NOT_SUPPORTED:
            sqlstate = "0A000";
            break;
        default:
            if (sqlstate.empty()) {
                sqlstate = "XX000";
            }
            break;
    }
}

void PostgresqlAdapter::updateTransactionStatus(const std::string& sql, bool has_error) {
    auto ltrim_upper = [](const std::string& input) {
        size_t pos = 0;
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
            ++pos;
        }
        std::string upper;
        upper.reserve(input.size() - pos);
        for (; pos < input.size(); ++pos) {
            upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(input[pos]))));
        }
        return upper;
    };

    std::string normalized = ltrim_upper(sql);
    auto starts_with = [&](const std::string& prefix) {
        return normalized.rfind(prefix, 0) == 0;
    };

    if (starts_with("BEGIN") || starts_with("START TRANSACTION")) {
        in_transaction_ = true;
        txn_failed_ = false;
        return;
    }
    if (starts_with("COMMIT")) {
        in_transaction_ = false;
        txn_failed_ = false;
        return;
    }
    if (starts_with("ROLLBACK")) {
        in_transaction_ = false;
        txn_failed_ = false;
        return;
    }

    if (has_error && in_transaction_) {
        txn_failed_ = true;
        return;
    }

    if (!in_transaction_) {
        txn_failed_ = false;
    }
}

std::string PostgresqlAdapter::computeMD5Hash(const std::string& password,
                                               const std::string& username,
                                               const uint8_t salt[4]) {
    // C1: MD5 auth validation - Compute PostgreSQL MD5 password hash
    // PostgreSQL MD5 format: md5(md5(password + username) + salt)
    // Result is "md5" prefix + 32 hex characters

#ifdef HAVE_OPENSSL
    // First hash: MD5(password + username)
    std::string input1 = password + username;
    unsigned char hash1[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(input1.c_str()), input1.length(), hash1);

    // Convert to hex
    std::stringstream ss1;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        ss1 << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash1[i]);
    }
    std::string hex1 = ss1.str();

    // Second hash: MD5(hex1 + salt)
    std::string input2 = hex1 + std::string(reinterpret_cast<const char*>(salt), 4);
    unsigned char hash2[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(input2.c_str()), input2.length(), hash2);

    // Convert to hex and prepend "md5"
    std::stringstream ss2;
    ss2 << "md5";
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        ss2 << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash2[i]);
    }
    return ss2.str();
#else
    // Fallback: simple hash for testing
    // In production, OpenSSL should always be available
    (void)password;
    (void)username;
    (void)salt;
    return "md5" + std::string(32, '0');
#endif
}

bool PostgresqlAdapter::validateMD5Response(const std::string& response,
                                             const std::string& expected_hash) {
    // C1: MD5 auth validation - Validate client response against expected hash
    // Response should be exactly 35 characters: "md5" + 32 hex digits
    
    if (response.length() != 35) {
        return false;
    }
    
    if (response.substr(0, 3) != "md5") {
        return false;
    }
    
    // Compare with expected hash (case-insensitive hex comparison)
    std::string response_lower = response;
    std::string expected_lower = expected_hash;
    
    std::transform(response_lower.begin(), response_lower.end(), 
                   response_lower.begin(), ::tolower);
    std::transform(expected_lower.begin(), expected_lower.end(), 
                   expected_lower.begin(), ::tolower);
    
    return response_lower == expected_lower;
}

} // namespace protocol
} // namespace scratchbird
