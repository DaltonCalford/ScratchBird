/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/udr/firebird_emulation_udr.h"

#include "scratchbird/catalog/emulation_view_generator.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/emulation_package_manifest.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::udr
{

    namespace
    {
        constexpr std::string_view kDefaultFirebirdServerToken = "firebird_localhost";

        void appendFirebirdDebug(const std::string& line)
        {
            std::ofstream out("/tmp/sb_ipc_debug.log", std::ios::app);
            if (!out)
            {
                return;
            }
            out << line << '\n';
        }

        void clearErrorContextSuccess(core::ErrorContext *ctx)
        {
            if (ctx == nullptr)
            {
                return;
            }
            ctx->code = core::Status::OK;
            ctx->sqlstate = core::statusToSQLState(core::Status::OK);
            ctx->sqlstate_text.clear();
            ctx->message.clear();
            ctx->vnext_code.clear();
            ctx->file = nullptr;
            ctx->line = 0;
            ctx->function = nullptr;
        }

        struct FirebirdDatabaseSpec
        {
            std::string server;
            std::string file_path;
        };

        auto requireFirebirdEmulationPackage(const std::string &profile_id,
                                             core::ErrorContext *ctx) -> core::Status
        {
            const core::EmulationPackageManifest *manifest = nullptr;
            return core::resolveInstalledEmulationPackage(profile_id,
                                                          core::EmulationPackageKind::EMULATION_UDR,
                                                          manifest,
                                                          ctx);
        }

        auto parseFirebirdDatabaseSpec(std::string_view spec) -> FirebirdDatabaseSpec
        {
            FirebirdDatabaseSpec result;
            result.file_path = std::string(spec);

            size_t colon = result.file_path.find(':');
            if (colon != std::string::npos)
            {
                const bool is_drive = (colon == 1 &&
                                       std::isalpha(static_cast<unsigned char>(result.file_path[0])) &&
                                       result.file_path.size() > 2 &&
                                       (result.file_path[2] == '\\' || result.file_path[2] == '/'));
                if (!is_drive)
                {
                    result.server = result.file_path.substr(0, colon);
                    result.file_path.erase(0, colon + 1);
                }
            }

            return result;
        }

        auto splitFirebirdPathComponents(std::string_view path) -> std::vector<std::string>
        {
            std::string working(path);
            std::vector<std::string> components;

            if (working.size() >= 2 && std::isalpha(static_cast<unsigned char>(working[0])) &&
                working[1] == ':')
            {
                std::string drive(
                    1, static_cast<char>(std::tolower(static_cast<unsigned char>(working[0]))));
                components.push_back(drive);
                working.erase(0, 2);
            }

            while (!working.empty() && (working.front() == '/' || working.front() == '\\'))
            {
                working.erase(working.begin());
            }

            std::string current;
            for (char ch : working)
            {
                if (ch == '/' || ch == '\\')
                {
                    if (!current.empty())
                    {
                        components.push_back(current);
                        current.clear();
                    }
                }
                else
                {
                    current.push_back(ch);
                }
            }
            if (!current.empty())
            {
                components.push_back(current);
            }

            if (!components.empty())
            {
                components.pop_back();
            }

            return components;
        }

        auto deriveFirebirdDatabaseName(std::string_view file_path) -> std::string
        {
            const size_t last_sep = file_path.find_last_of("/\\");
            std::string base = (last_sep == std::string_view::npos)
                                   ? std::string(file_path)
                                   : std::string(file_path.substr(last_sep + 1));

            if (base.empty())
            {
                return base;
            }

            const size_t dot = base.find_last_of('.');
            if (dot != std::string::npos && dot + 1 < base.size())
            {
                std::string ext = base.substr(dot + 1);
                for (char &ch : ext)
                {
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                }
                if (ext == "fdb" || ext == "gdb" || ext == "sbdb")
                {
                    base = base.substr(0, dot);
                }
            }

            return base;
        }

        auto buildEmulatedFirebirdSchemaPath(const std::string &server,
                                             const std::vector<std::string> &path_components,
                                             const std::string &db_name) -> std::string
        {
            std::string schema = "emulated.firebird." + server;
            for (const auto &comp : path_components)
            {
                if (!comp.empty())
                {
                    schema.push_back('.');
                    schema += comp;
                }
            }
            if (!db_name.empty())
            {
                schema.push_back('.');
                schema += db_name;
            }
            return schema;
        }

        auto buildLegacyEmulatedFirebirdSchemaPath(const std::string &server,
                                                   const std::vector<std::string> &path_components,
                                                   const std::string &db_name) -> std::string
        {
            std::string schema = "remote.emulation.firebird." + server;
            for (const auto &comp : path_components)
            {
                if (!comp.empty())
                {
                    schema.push_back('.');
                    schema += comp;
                }
            }
            if (!db_name.empty())
            {
                schema.push_back('.');
                schema += db_name;
            }
            return schema;
        }

        auto escapeLiteral(const std::string &in) -> std::string
        {
            std::string out;
            out.reserve(in.size() + 8);
            for (char c : in)
            {
                out.push_back(c);
                if (c == '\'')
                {
                    out.push_back('\'');
                }
            }
            return out;
        }

        auto buildLegacySchemaName(std::string_view schema_name) -> std::string
        {
            static constexpr std::string_view kCanonicalPrefix = "emulated.firebird.";
            static constexpr std::string_view kCompatibilityPrefix = "emulation.firebird.";
            static constexpr std::string_view kLegacyPrefix = "remote.emulation.firebird.";

            if (schema_name.rfind(kCanonicalPrefix, 0) == 0)
            {
                return std::string(kLegacyPrefix) +
                       std::string(schema_name.substr(kCanonicalPrefix.size()));
            }
            if (schema_name.rfind(kCompatibilityPrefix, 0) == 0)
            {
                return std::string(kLegacyPrefix) +
                       std::string(schema_name.substr(kCompatibilityPrefix.size()));
            }
            return std::string(schema_name);
        }

        auto deriveFirebirdVirtualCatalogBinding(const FirebirdVirtualCatalogRequest &request,
                                                 FirebirdVirtualCatalogResponse &response,
                                                 core::ErrorContext *ctx) -> core::Status
        {
            if (!request.database_binding.empty())
            {
                FirebirdSchemaBindingRequest binding_request{};
                binding_request.profile_id = request.profile_id;
                binding_request.database_binding = request.database_binding;
                return deriveFirebirdSchemaBinding(binding_request, response, ctx);
            }

            if (request.schema_name.empty())
            {
                if (ctx != nullptr)
                {
                    ctx->set(core::Status::INVALID_ARGUMENT,
                             "Firebird virtual catalog request requires a database binding or schema name",
                             __FILE__,
                             __LINE__,
                             __func__);
                }
                return core::Status::INVALID_ARGUMENT;
            }

            response.schema_name = request.schema_name;
            response.server_name = request.server_name.empty()
                                       ? std::string(kDefaultFirebirdServerToken)
                                       : request.server_name;
            response.database_name = request.database_name.empty() ? "default"
                                                                   : request.database_name;
            response.legacy_schema_name = buildLegacySchemaName(response.schema_name);
            clearErrorContextSuccess(ctx);
            return core::Status::OK;
        }

        auto resolveFirebirdSchema(core::CatalogManager *catalog,
                                   FirebirdVirtualCatalogResponse &response,
                                   core::CatalogManager::SchemaInfo &schema_out,
                                   core::ErrorContext *ctx) -> core::Status
        {
            core::ErrorContext schema_probe_ctx;
            auto status = catalog->getSchema(response.schema_name, schema_out, &schema_probe_ctx);
            if (status != core::Status::OK &&
                status != core::Status::INVALID_ARGUMENT &&
                status != core::Status::NOT_FOUND)
            {
                if (ctx != nullptr && !schema_probe_ctx.message.empty())
                {
                    ctx->set(schema_probe_ctx.code,
                             schema_probe_ctx.message.c_str(),
                             schema_probe_ctx.file,
                             schema_probe_ctx.line,
                             schema_probe_ctx.function);
                }
                return status;
            }

            if (status != core::Status::OK && !response.legacy_schema_name.empty() &&
                response.legacy_schema_name != response.schema_name)
            {
                core::ErrorContext legacy_ctx;
                if (catalog->getSchema(response.legacy_schema_name, schema_out, &legacy_ctx) ==
                    core::Status::OK)
                {
                    response.schema_name = response.legacy_schema_name;
                    return core::Status::OK;
                }
            }

            return status;
        }
    } // namespace

    auto ensureFirebirdVirtualCatalog(core::Database *database,
                                      const FirebirdVirtualCatalogRequest &request,
                                      FirebirdVirtualCatalogResponse &response,
                                      core::ErrorContext *ctx) -> core::Status
    {
        auto status = requireFirebirdEmulationPackage(request.profile_id, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        if (database == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Database not initialized",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        auto *catalog = database->catalog_manager();
        if (catalog == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Catalog manager not available",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        auto binding_status = deriveFirebirdVirtualCatalogBinding(request, response, ctx);
        if (binding_status != core::Status::OK)
        {
            return binding_status;
        }

        core::CatalogManager::SchemaInfo fb_schema;
        status = resolveFirebirdSchema(catalog, response, fb_schema, ctx);
        if (status != core::Status::OK)
        {
            if (status != core::Status::INVALID_ARGUMENT && status != core::Status::NOT_FOUND)
            {
                return status;
            }
            core::ID schema_id;
            status = catalog->createSchemaPath(response.schema_name,
                                               core::CatalogManager::SchemaType::REMOTE_EMULATED,
                                               schema_id,
                                               ctx);
            if (status != core::Status::OK)
            {
                return status;
            }
            status = catalog->getSchema(schema_id, fb_schema, ctx);
            if (status != core::Status::OK)
            {
                return status;
            }
        }
        response.schema_id = fb_schema.schema_id;
        {
            std::string resolved_schema_path;
            core::ErrorContext path_ctx;
            if (catalog->getSchemaPath(fb_schema.schema_id, resolved_schema_path, &path_ctx) !=
                    core::Status::OK ||
                resolved_schema_path.empty())
            {
                resolved_schema_path =
                    fb_schema.full_path.empty() ? fb_schema.schema_name : fb_schema.full_path;
            }
            std::ostringstream trace;
            trace << "[ipc_debug] firebird virtual catalog schema_name="
                  << response.schema_name
                  << " schema_id=" << fb_schema.schema_id.toString()
                  << " schema_path="
                  << resolved_schema_path;
            appendFirebirdDebug(trace.str());
        }

        {
            catalog::EmulationViewGenerator generator(catalog);
            status = generator.generateEmulatedViews(response.schema_name,
                                                    response.server_name,
                                                    response.database_name,
                                                    catalog::ProtocolType::FIREBIRD,
                                                    ctx);
            if (status != core::Status::OK)
            {
                return status;
            }
            clearErrorContextSuccess(ctx);
            return core::Status::OK;
        }

        auto ensure_view = [&](const std::string &name,
                               const std::string &definition,
                               const std::vector<std::string> &column_names = {},
                               bool refresh_if_changed = false)
            -> core::Status
        {
            const bool trace_view = name == "RDB$RELATIONS" || name == "RDB$FIELDS" ||
                                    name == "RDB$RELATION_FIELDS" ||
                                    name == "RDB$PROCEDURES" ||
                                    name == "RDB$PROCEDURE_PARAMETERS";
            core::CatalogManager::ViewInfo view_info;
            core::ErrorContext probe_ctx;
            auto s = catalog->getView(fb_schema.schema_id, name, view_info, &probe_ctx);
            if (s == core::Status::OK)
            {
                if (trace_view)
                {
                    std::ostringstream trace;
                    trace << "[ipc_debug] firebird ensure_view existing name=" << name
                          << " current_len=" << view_info.definition.size()
                          << " new_len=" << definition.size()
                          << " current_cols=" << view_info.column_names.size()
                          << " new_cols=" << column_names.size();
                    appendFirebirdDebug(trace.str());
                }
                if (!refresh_if_changed)
                {
                    return core::Status::OK;
                }

                if (view_info.definition == definition && view_info.column_names == column_names &&
                    !view_info.materialized && !view_info.check_option)
                {
                    if (trace_view)
                    {
                        std::ostringstream trace;
                        trace << "[ipc_debug] firebird ensure_view unchanged name=" << name;
                        appendFirebirdDebug(trace.str());
                    }
                    return core::Status::OK;
                }

                core::ErrorContext replace_ctx;
                const auto replace_status = catalog->createView(fb_schema.schema_id,
                                                                name,
                                                                definition,
                                                                true,
                                                                false,
                                                                false,
                                                                column_names,
                                                                core::ID{},
                                                                &replace_ctx);
                if (replace_status != core::Status::OK && ctx != nullptr &&
                    !replace_ctx.message.empty())
                {
                    ctx->set(replace_ctx.code,
                             replace_ctx.message.c_str(),
                             replace_ctx.file,
                             replace_ctx.line,
                             replace_ctx.function);
                }
                if (trace_view)
                {
                    core::CatalogManager::ViewInfo refreshed_view;
                    core::ErrorContext refreshed_ctx;
                    const auto refreshed_status =
                        catalog->getView(fb_schema.schema_id, name, refreshed_view, &refreshed_ctx);
                    std::ostringstream trace;
                    trace << "[ipc_debug] firebird ensure_view replace name=" << name
                          << " status=" << static_cast<int>(replace_status)
                          << " refreshed_status=" << static_cast<int>(refreshed_status)
                          << " stored_len="
                          << (refreshed_status == core::Status::OK
                                  ? refreshed_view.definition.size()
                                  : 0)
                          << " stored_cols="
                          << (refreshed_status == core::Status::OK
                                  ? refreshed_view.column_names.size()
                                  : 0);
                    appendFirebirdDebug(trace.str());
                }
                return replace_status;
            }
            if (s != core::Status::INVALID_ARGUMENT && s != core::Status::NOT_FOUND)
            {
                if (ctx != nullptr && !probe_ctx.message.empty())
                {
                    ctx->set(probe_ctx.code,
                             probe_ctx.message.c_str(),
                             probe_ctx.file,
                             probe_ctx.line,
                             probe_ctx.function);
                }
                return s;
            }
            if (trace_view)
            {
                std::ostringstream trace;
                trace << "[ipc_debug] firebird ensure_view create name=" << name
                      << " new_len=" << definition.size()
                      << " new_cols=" << column_names.size();
                appendFirebirdDebug(trace.str());
            }
            core::ErrorContext create_ctx;
            const auto create_status = catalog->createView(fb_schema.schema_id,
                                                           name,
                                                           definition,
                                                           false,
                                                           false,
                                                           false,
                                                           column_names,
                                                           core::ID{},
                                                           &create_ctx);
            if (create_status != core::Status::OK && ctx != nullptr &&
                !create_ctx.message.empty())
            {
                ctx->set(create_ctx.code,
                         create_ctx.message.c_str(),
                         create_ctx.file,
                         create_ctx.line,
                         create_ctx.function);
            }
            if (trace_view)
            {
                core::CatalogManager::ViewInfo refreshed_view;
                core::ErrorContext refreshed_ctx;
                const auto refreshed_status =
                    catalog->getView(fb_schema.schema_id, name, refreshed_view, &refreshed_ctx);
                std::ostringstream trace;
                trace << "[ipc_debug] firebird ensure_view create_result name=" << name
                      << " status=" << static_cast<int>(create_status)
                      << " refreshed_status=" << static_cast<int>(refreshed_status)
                      << " stored_len="
                      << (refreshed_status == core::Status::OK ? refreshed_view.definition.size()
                                                               : 0)
                      << " stored_cols="
                      << (refreshed_status == core::Status::OK
                              ? refreshed_view.column_names.size()
                              : 0);
                appendFirebirdDebug(trace.str());
            }
            return create_status;
        };

        status = ensure_view("RDB$DATABASE", "SELECT 1 AS DUMMY", {"DUMMY"});
        if (status != core::Status::OK)
        {
            return status;
        }

        std::vector<core::CatalogManager::SchemaInfo> schema_scope;
        schema_scope.push_back(fb_schema);
        {
            std::vector<core::CatalogManager::SchemaInfo> all_schemas;
            const auto schema_status = catalog->listSchemas(all_schemas, ctx);
            if (schema_status == core::Status::OK)
            {
                const std::string root_path =
                    fb_schema.full_path.empty() ? response.schema_name : fb_schema.full_path;
                const std::string root_prefix = root_path + ".";
                for (const auto &schema : all_schemas)
                {
                    if (schema.schema_id == fb_schema.schema_id)
                    {
                        continue;
                    }

                    const std::string candidate_path =
                        schema.full_path.empty() ? schema.schema_name : schema.full_path;
                    if (candidate_path.rfind(root_prefix, 0) == 0)
                    {
                        schema_scope.push_back(schema);
                    }
                }
            }
        }

        std::unordered_set<core::ID, core::IDHash> schema_scope_ids;
        for (const auto &schema_info : schema_scope)
        {
            schema_scope_ids.insert(schema_info.schema_id);
        }

        std::vector<core::CatalogManager::TableInfo> tables;
        for (const auto &schema_info : schema_scope)
        {
            std::vector<core::CatalogManager::TableInfo> schema_tables;
            const auto table_status = catalog->listTables(schema_info.schema_id, schema_tables, ctx);
            {
                std::ostringstream trace;
                trace << "[ipc_debug] firebird virtual catalog listTables schema_id="
                      << schema_info.schema_id.toString()
                      << " schema_path="
                      << (schema_info.full_path.empty() ? schema_info.schema_name
                                                        : schema_info.full_path)
                      << " status=" << static_cast<int>(table_status)
                      << " table_count=" << schema_tables.size();
                appendFirebirdDebug(trace.str());
            }
            for (const auto &schema_table : schema_tables)
            {
                std::ostringstream trace;
                trace << "[ipc_debug] firebird virtual catalog table schema_id="
                      << schema_table.schema_id.toString()
                      << " table=" << schema_table.table_name
                      << " table_id=" << schema_table.table_id.toString();
                appendFirebirdDebug(trace.str());
            }
            tables.insert(tables.end(), schema_tables.begin(), schema_tables.end());
        }

        std::unordered_map<core::ID, core::CatalogManager::TableInfo, core::IDHash> table_by_id;
        std::unordered_map<std::string, std::string> table_by_name;
        for (const auto &t : tables)
        {
            table_by_id.emplace(t.table_id, t);
            std::string key = t.table_name;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            table_by_name.emplace(key, t.table_name);
        }

        std::unordered_map<core::ID, std::vector<core::CatalogManager::ColumnInfo>, core::IDHash>
            columns_by_table;
        for (const auto &t : tables)
        {
            std::vector<core::CatalogManager::ColumnInfo> cols;
            catalog->getColumns(t.table_id, cols, ctx);
            columns_by_table.emplace(t.table_id, std::move(cols));
        }

        std::vector<core::CatalogManager::ViewInfo> views;
        for (const auto &schema_info : schema_scope)
        {
            std::vector<core::CatalogManager::ViewInfo> schema_views;
            const auto s = catalog->listViewsForSchema(schema_info.schema_id, schema_views, ctx);
            if (s != core::Status::OK)
            {
                continue;
            }
            views.insert(views.end(), schema_views.begin(), schema_views.end());
        }

        std::vector<core::CatalogManager::ProcedureInfo> procedures;
        {
            const auto s = catalog->listProcedures(procedures, ctx);
            if (s != core::Status::OK)
            {
                procedures.clear();
            }
            else
            {
                procedures.erase(std::remove_if(procedures.begin(),
                                                procedures.end(),
                                                [&](const auto &proc) {
                                                    return schema_scope_ids.find(proc.schema_id) ==
                                                           schema_scope_ids.end();
                                                }),
                                 procedures.end());
            }
        }

        const auto make_firebird_field_source = [](std::string_view relation_name,
                                                   std::string_view field_name) {
            std::string result;
            result.reserve(relation_name.size() + field_name.size() + 1);
            result.append(relation_name);
            result.push_back('$');
            result.append(field_name);
            return result;
        };

        const auto firebird_field_type_for = [](core::DataType data_type, bool is_array) -> int {
            if (is_array)
            {
                return 9;
            }
            switch (data_type)
            {
                case core::DataType::BOOLEAN:
                    return 23;
                case core::DataType::INT8:
                case core::DataType::INT16:
                    return 7;
                case core::DataType::INT32:
                case core::DataType::UINT8:
                case core::DataType::UINT16:
                case core::DataType::UINT32:
                case core::DataType::MEDIUMINT:
                    return 8;
                case core::DataType::INT64:
                case core::DataType::UINT64:
                case core::DataType::DECIMAL:
                case core::DataType::MONEY:
                    return 16;
                case core::DataType::INT128:
                case core::DataType::UINT128:
                    return 26;
                case core::DataType::DECFLOAT16:
                    return 24;
                case core::DataType::DECFLOAT34:
                    return 25;
                case core::DataType::FLOAT32:
                    return 10;
                case core::DataType::FLOAT64:
                    return 27;
                case core::DataType::CHAR:
                    return 14;
                case core::DataType::VARCHAR:
                case core::DataType::TEXT:
                case core::DataType::JSON:
                case core::DataType::JSONB:
                case core::DataType::XML:
                case core::DataType::UUID:
                    return 37;
                case core::DataType::BINARY:
                    return 14;
                case core::DataType::VARBINARY:
                    return 37;
                case core::DataType::BLOB:
                case core::DataType::BLOB_SUB_TYPE_TEXT:
                case core::DataType::BYTEA:
                    return 261;
                case core::DataType::DATE:
                    return 12;
                case core::DataType::TIME:
                    return 13;
                case core::DataType::TIME_WITH_ZONE:
                    return 28;
                case core::DataType::TIMESTAMP:
                case core::DataType::DATETIME:
                    return 35;
                case core::DataType::TIMESTAMP_WITH_ZONE:
                    return 29;
                case core::DataType::ARRAY:
                    return 9;
                default:
                    return 37;
            }
        };

        std::string rel_sql;
        if (tables.empty() && views.empty())
        {
            rel_sql =
                "SELECT NULL AS RDB$RELATION_NAME, NULL AS RDB$SYSTEM_FLAG, NULL AS "
                "RDB$VIEW_BLR, NULL AS RDB$VIEW_SOURCE, NULL AS RDB$OWNER_NAME WHERE 1 = 0";
        }
        else
        {
            std::ostringstream ss;
            bool first = true;
            for (const auto &t : tables)
            {
                if (!first)
                {
                    ss << " UNION ALL ";
                }
                ss << "SELECT '" << escapeLiteral(t.table_name)
                   << "' AS RDB$RELATION_NAME, 0 AS RDB$SYSTEM_FLAG, NULL AS RDB$VIEW_BLR, "
                   << "NULL AS RDB$VIEW_SOURCE, NULL AS RDB$OWNER_NAME";
                first = false;
            }
            for (const auto &v : views)
            {
                if (!first)
                {
                    ss << " UNION ALL ";
                }
                ss << "SELECT '" << escapeLiteral(v.name)
                   << "' AS RDB$RELATION_NAME, 0 AS RDB$SYSTEM_FLAG, NULL AS RDB$VIEW_BLR, "
                   << (v.definition.empty() ? "NULL"
                                            : ("'" + escapeLiteral(v.definition) + "'"))
                   << " AS RDB$VIEW_SOURCE, NULL AS RDB$OWNER_NAME";
                first = false;
            }
            rel_sql = ss.str();
        }
        status = ensure_view("RDB$RELATIONS",
                             rel_sql,
                             {"RDB$RELATION_NAME",
                              "RDB$SYSTEM_FLAG",
                              "RDB$VIEW_BLR",
                              "RDB$VIEW_SOURCE",
                              "RDB$OWNER_NAME"},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }

        std::string rel_fields_sql;
        std::string fields_sql;
        {
            std::ostringstream rf, f;
            bool rf_first = true;
            bool f_first = true;
            std::unordered_set<std::string> seen_fields;
            const auto append_field_definition =
                [&](const std::string &field_source,
                    core::DataType data_type,
                    uint32_t type_precision,
                    uint32_t type_scale,
                    uint32_t max_length,
                    bool nullable,
                    uint16_t charset_id,
                    uint32_t collation_id,
                    bool is_array,
                    const std::string &default_value) {
                    if (!seen_fields.insert(field_source).second)
                    {
                        return;
                    }
                    if (!f_first)
                    {
                        f << " UNION ALL ";
                    }
                    const uint32_t length =
                        type_precision ? type_precision : (max_length ? max_length : 0);
                    f << "SELECT '" << escapeLiteral(field_source)
                      << "' AS RDB$FIELD_NAME, "
                      << firebird_field_type_for(data_type, is_array)
                      << " AS RDB$FIELD_TYPE, 0 AS RDB$FIELD_SUB_TYPE, " << length
                      << " AS RDB$FIELD_LENGTH, " << length
                      << " AS RDB$SEGMENT_LENGTH, " << static_cast<int32_t>(type_scale)
                      << " AS RDB$FIELD_SCALE, " << charset_id
                      << " AS RDB$CHARACTER_SET_ID, " << collation_id
                      << " AS RDB$COLLATION_ID, " << (is_array ? 1 : 0)
                      << " AS RDB$DIMENSIONS, " << (nullable ? 0 : 1)
                      << " AS RDB$NULL_FLAG, "
                      << (default_value.empty() ? "NULL"
                                                : ("'" + escapeLiteral(default_value) + "'"))
                      << " AS RDB$DEFAULT_SOURCE";
                    f_first = false;
                };
            for (const auto &t : tables)
            {
                const auto it = columns_by_table.find(t.table_id);
                if (it == columns_by_table.end())
                {
                    continue;
                }
                const auto &cols = it->second;
                for (const auto &col : cols)
                {
                    if (!rf_first)
                    {
                        rf << " UNION ALL ";
                    }
                    const std::string field_source =
                        make_firebird_field_source(t.table_name, col.column_name);
                    rf << "SELECT '" << escapeLiteral(col.column_name) << "' AS RDB$FIELD_NAME, '"
                       << escapeLiteral(t.table_name) << "' AS RDB$RELATION_NAME, '"
                       << escapeLiteral(field_source) << "' AS RDB$FIELD_SOURCE, "
                       << static_cast<int>(col.ordinal) << " AS RDB$FIELD_POSITION, "
                       << (col.nullable ? 0 : 1) << " AS RDB$NULL_FLAG";
                    rf_first = false;
                    append_field_definition(field_source,
                                            static_cast<core::DataType>(col.data_type),
                                            col.type_precision,
                                            col.type_scale,
                                            col.max_length,
                                            col.nullable,
                                            col.charset,
                                            col.collation_id,
                                            col.is_array,
                                            col.default_value);
                }
            }
            for (const auto &proc : procedures)
            {
                for (const auto &p : proc.parameters)
                {
                    append_field_definition(make_firebird_field_source(proc.name, p.name),
                                            p.type,
                                            p.type_precision,
                                            p.type_scale,
                                            p.type_precision,
                                            true,
                                            0,
                                            0,
                                            false,
                                            p.has_default ? p.default_value : std::string{});
                }
            }
            if (rf_first)
            {
                rel_fields_sql = "SELECT NULL AS RDB$FIELD_NAME, NULL AS RDB$RELATION_NAME, "
                                 "NULL AS RDB$FIELD_SOURCE, NULL AS RDB$FIELD_POSITION, NULL "
                                 "AS RDB$NULL_FLAG WHERE 1 = 0";
            }
            else
            {
                rel_fields_sql = rf.str();
            }
            if (f_first)
            {
                fields_sql =
                    "SELECT NULL AS RDB$FIELD_NAME, NULL AS RDB$FIELD_TYPE, NULL AS "
                    "RDB$FIELD_SUB_TYPE, NULL AS RDB$FIELD_LENGTH, NULL AS "
                    "RDB$SEGMENT_LENGTH, NULL AS RDB$FIELD_SCALE, NULL AS "
                    "RDB$CHARACTER_SET_ID, NULL AS RDB$COLLATION_ID, NULL AS RDB$DIMENSIONS, "
                    "NULL AS RDB$NULL_FLAG, NULL AS RDB$DEFAULT_SOURCE WHERE 1 = 0";
            }
            else
            {
                fields_sql = f.str();
            }
        }

        std::string indices_sql;
        std::string index_segments_sql;
        {
            std::ostringstream idx_ss;
            std::ostringstream seg_ss;
            bool idx_first = true;
            bool seg_first = true;
            for (const auto &t : tables)
            {
                std::vector<core::CatalogManager::IndexInfo> indexes;
                const auto s = catalog->listIndexesForTable(t.table_id, indexes, ctx);
                if (s != core::Status::OK)
                {
                    continue;
                }
                const auto col_it = columns_by_table.find(t.table_id);
                for (const auto &idx : indexes)
                {
                    if (!idx_first)
                    {
                        idx_ss << " UNION ALL ";
                    }
                    idx_ss << "SELECT '" << escapeLiteral(idx.index_name)
                           << "' AS RDB$INDEX_NAME, '" << escapeLiteral(t.table_name)
                           << "' AS RDB$RELATION_NAME, " << (idx.is_unique ? 1 : 0)
                           << " AS RDB$UNIQUE_FLAG, 0 AS RDB$INDEX_TYPE";
                    idx_first = false;

                    if (col_it == columns_by_table.end())
                    {
                        continue;
                    }
                    for (size_t pos = 0; pos < idx.column_ids.size(); ++pos)
                    {
                        const auto &col_id = idx.column_ids[pos];
                        std::string col_name = "COLUMN_" + std::to_string(pos);
                        for (const auto &col : col_it->second)
                        {
                            if (col.column_id == col_id)
                            {
                                col_name = col.column_name;
                                break;
                            }
                        }
                        if (!seg_first)
                        {
                            seg_ss << " UNION ALL ";
                        }
                        seg_ss << "SELECT '" << escapeLiteral(idx.index_name)
                               << "' AS RDB$INDEX_NAME, '" << escapeLiteral(col_name)
                               << "' AS RDB$FIELD_NAME, " << static_cast<int>(pos)
                               << " AS RDB$FIELD_POSITION";
                        seg_first = false;
                    }
                }
            }

            if (idx_first)
            {
                indices_sql = "SELECT NULL AS RDB$INDEX_NAME, NULL AS RDB$RELATION_NAME, NULL AS "
                              "RDB$UNIQUE_FLAG, NULL AS RDB$INDEX_TYPE WHERE 1 = 0";
            }
            else
            {
                indices_sql = idx_ss.str();
            }

            if (seg_first)
            {
                index_segments_sql = "SELECT NULL AS RDB$INDEX_NAME, NULL AS RDB$FIELD_NAME, "
                                     "NULL AS RDB$FIELD_POSITION WHERE 1 = 0";
            }
            else
            {
                index_segments_sql = seg_ss.str();
            }
        }

        std::string relation_constraints_sql;
        std::string check_constraints_sql;
        std::string ref_constraints_sql;
        {
            std::ostringstream rc, cc, rf;
            bool rc_first = true;
            bool cc_first = true;
            bool rf_first = true;

            const auto constraint_type =
                [](core::CatalogManager::ConstraintType type) -> std::string {
                switch (type)
                {
                    case core::CatalogManager::ConstraintType::PRIMARY_KEY:
                        return "PRIMARY KEY";
                    case core::CatalogManager::ConstraintType::UNIQUE:
                        return "UNIQUE";
                    case core::CatalogManager::ConstraintType::FOREIGN_KEY:
                        return "FOREIGN KEY";
                    case core::CatalogManager::ConstraintType::CHECK:
                        return "CHECK";
                    case core::CatalogManager::ConstraintType::NOT_NULL:
                        return "NOT NULL";
                    case core::CatalogManager::ConstraintType::EXCLUSION:
                        return "EXCLUSION";
                }
                return "UNKNOWN";
            };

            const auto fk_action = [](core::CatalogManager::FKAction action) -> std::string {
                switch (action)
                {
                    case core::CatalogManager::FKAction::NO_ACTION:
                        return "NO ACTION";
                    case core::CatalogManager::FKAction::RESTRICT:
                        return "RESTRICT";
                    case core::CatalogManager::FKAction::CASCADE:
                        return "CASCADE";
                    case core::CatalogManager::FKAction::SET_NULL:
                        return "SET NULL";
                    case core::CatalogManager::FKAction::SET_DEFAULT:
                        return "SET DEFAULT";
                }
                return "NO ACTION";
            };

            const auto fk_match = [](core::CatalogManager::FKMatchType match) -> std::string {
                switch (match)
                {
                    case core::CatalogManager::FKMatchType::FULL:
                        return "FULL";
                    case core::CatalogManager::FKMatchType::PARTIAL:
                        return "PARTIAL";
                    case core::CatalogManager::FKMatchType::SIMPLE:
                    default:
                        return "SIMPLE";
                }
            };

            const auto find_matching_constraint =
                [&](const core::CatalogManager::ConstraintInfo &fk) -> std::string {
                if (fk.referenced_table_id == core::ID{})
                {
                    return {};
                }
                std::vector<core::CatalogManager::ConstraintInfo> target;
                if (catalog->getConstraintsByType(fk.referenced_table_id,
                                                  core::CatalogManager::ConstraintType::PRIMARY_KEY,
                                                  target,
                                                  ctx) != core::Status::OK)
                {
                    target.clear();
                }
                if (target.empty())
                {
                    catalog->getConstraintsByType(fk.referenced_table_id,
                                                  core::CatalogManager::ConstraintType::UNIQUE,
                                                  target,
                                                  ctx);
                }
                for (const auto &c : target)
                {
                    if (c.column_names == fk.referenced_columns)
                    {
                        return c.constraint_name;
                    }
                }
                return {};
            };

            for (const auto &t : tables)
            {
                std::vector<core::CatalogManager::ConstraintInfo> constraints;
                const auto s = catalog->getConstraintsForTable(t.table_id, constraints, ctx);
                if (s != core::Status::OK)
                {
                    continue;
                }

                for (const auto &c : constraints)
                {
                    if (!rc_first)
                    {
                        rc << " UNION ALL ";
                    }
                    rc << "SELECT '" << escapeLiteral(c.constraint_name)
                       << "' AS RDB$CONSTRAINT_NAME, '" << constraint_type(c.constraint_type)
                       << "' AS RDB$CONSTRAINT_TYPE, '" << escapeLiteral(t.table_name)
                       << "' AS RDB$RELATION_NAME, '" << escapeLiteral(c.constraint_name)
                       << "' AS RDB$INDEX_NAME, " << (c.is_deferrable ? 1 : 0)
                       << " AS RDB$DEFERRABLE, " << (c.initially_deferred ? 1 : 0)
                       << " AS RDB$INITIALLY_DEFERRED";
                    rc_first = false;

                    if (c.constraint_type == core::CatalogManager::ConstraintType::CHECK)
                    {
                        if (!cc_first)
                        {
                            cc << " UNION ALL ";
                        }
                        cc << "SELECT '" << escapeLiteral(c.constraint_name)
                           << "' AS RDB$CONSTRAINT_NAME, '" << escapeLiteral(c.constraint_name)
                           << "' AS RDB$TRIGGER_NAME";
                        cc_first = false;
                    }
                    else if (c.constraint_type ==
                             core::CatalogManager::ConstraintType::FOREIGN_KEY)
                    {
                        const auto ref = find_matching_constraint(c);
                        if (!rf_first)
                        {
                            rf << " UNION ALL ";
                        }
                        rf << "SELECT '" << escapeLiteral(c.constraint_name)
                           << "' AS RDB$CONSTRAINT_NAME, "
                           << (ref.empty() ? "NULL" : ("'" + escapeLiteral(ref) + "'"))
                           << " AS RDB$CONST_NAME_UQ, '" << fk_match(c.match_type)
                           << "' AS RDB$MATCH_OPTION, '" << fk_action(c.on_update)
                           << "' AS RDB$UPDATE_RULE, '" << fk_action(c.on_delete)
                           << "' AS RDB$DELETE_RULE";
                        rf_first = false;
                    }
                }
            }

            if (rc_first)
            {
                relation_constraints_sql =
                    "SELECT NULL AS RDB$CONSTRAINT_NAME, NULL AS RDB$CONSTRAINT_TYPE, NULL AS "
                    "RDB$RELATION_NAME, NULL AS RDB$INDEX_NAME, NULL AS RDB$DEFERRABLE, NULL "
                    "AS RDB$INITIALLY_DEFERRED WHERE 1 = 0";
            }
            else
            {
                relation_constraints_sql = rc.str();
            }

            if (cc_first)
            {
                check_constraints_sql =
                    "SELECT NULL AS RDB$CONSTRAINT_NAME, NULL AS RDB$TRIGGER_NAME WHERE 1 = 0";
            }
            else
            {
                check_constraints_sql = cc.str();
            }

            if (rf_first)
            {
                ref_constraints_sql =
                    "SELECT NULL AS RDB$CONSTRAINT_NAME, NULL AS RDB$CONST_NAME_UQ, NULL AS "
                    "RDB$MATCH_OPTION, NULL AS RDB$UPDATE_RULE, NULL AS RDB$DELETE_RULE WHERE "
                    "1 = 0";
            }
            else
            {
                ref_constraints_sql = rf.str();
            }
        }

        status = ensure_view("RDB$FIELDS",
                             fields_sql,
                             {"RDB$FIELD_NAME",
                              "RDB$FIELD_TYPE",
                              "RDB$FIELD_SUB_TYPE",
                              "RDB$FIELD_LENGTH",
                              "RDB$SEGMENT_LENGTH",
                              "RDB$FIELD_SCALE",
                              "RDB$CHARACTER_SET_ID",
                              "RDB$COLLATION_ID",
                              "RDB$DIMENSIONS",
                              "RDB$NULL_FLAG",
                              "RDB$DEFAULT_SOURCE"},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }
        status = ensure_view("RDB$FORMATS",
                             "SELECT NULL AS RDB$FORMAT, NULL AS RDB$RELATION_ID WHERE 1 = 0",
                             {},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }
        status = ensure_view("RDB$TYPES",
                             "SELECT NULL AS RDB$TYPE, NULL AS RDB$FIELD_NAME WHERE 1 = 0",
                             {},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }
        status = ensure_view("RDB$RELATION_FIELDS",
                             rel_fields_sql,
                             {"RDB$FIELD_NAME",
                              "RDB$RELATION_NAME",
                              "RDB$FIELD_SOURCE",
                              "RDB$FIELD_POSITION",
                              "RDB$NULL_FLAG"},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }
        status = ensure_view("RDB$INDICES",
                             indices_sql,
                             {"RDB$INDEX_NAME",
                              "RDB$RELATION_NAME",
                              "RDB$UNIQUE_FLAG",
                              "RDB$INDEX_TYPE"},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }
        status = ensure_view("RDB$INDEX_SEGMENTS",
                             index_segments_sql,
                             {"RDB$INDEX_NAME", "RDB$FIELD_NAME", "RDB$FIELD_POSITION"},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }
        status = ensure_view("RDB$RELATION_CONSTRAINTS",
                             relation_constraints_sql,
                             {"RDB$CONSTRAINT_NAME",
                              "RDB$CONSTRAINT_TYPE",
                              "RDB$RELATION_NAME",
                              "RDB$INDEX_NAME",
                              "RDB$DEFERRABLE",
                              "RDB$INITIALLY_DEFERRED"},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }
        status = ensure_view("RDB$CHECK_CONSTRAINTS", check_constraints_sql, {}, true);
        if (status != core::Status::OK)
        {
            return status;
        }
        status = ensure_view("RDB$REF_CONSTRAINTS", ref_constraints_sql, {}, true);
        if (status != core::Status::OK)
        {
            return status;
        }

        std::string triggers_sql;
        {
            std::ostringstream tr;
            bool tr_first = true;
            for (const auto &t : tables)
            {
                std::vector<core::CatalogManager::TriggerInfo> triggers;
                const auto s = catalog->listAllTriggersForTable(t.table_id, triggers, ctx);
                if (s != core::Status::OK)
                {
                    continue;
                }
                int32_t seq = 0;
                for (const auto &trig : triggers)
                {
                    if (!trig.enabled)
                    {
                        continue;
                    }
                    const auto emit_trigger = [&](int32_t trigger_type) {
                        if (!tr_first)
                        {
                            tr << " UNION ALL ";
                        }
                        tr << "SELECT '" << escapeLiteral(trig.trigger_name)
                           << "' AS RDB$TRIGGER_NAME, '" << escapeLiteral(t.table_name)
                           << "' AS RDB$RELATION_NAME, " << seq++
                           << " AS RDB$TRIGGER_SEQUENCE, " << trigger_type
                           << " AS RDB$TRIGGER_TYPE";
                        tr_first = false;
                    };

                    const auto has_event = [&](core::CatalogManager::TriggerEvent event) {
                        return (trig.event_mask & (1u << static_cast<uint8_t>(event))) != 0;
                    };

                    if (trig.timing == core::CatalogManager::TriggerTiming::BEFORE)
                    {
                        if (has_event(core::CatalogManager::TriggerEvent::INSERT))
                        {
                            emit_trigger(1);
                        }
                        if (has_event(core::CatalogManager::TriggerEvent::UPDATE))
                        {
                            emit_trigger(3);
                        }
                        if (has_event(core::CatalogManager::TriggerEvent::DELETE))
                        {
                            emit_trigger(5);
                        }
                    }
                    else if (trig.timing == core::CatalogManager::TriggerTiming::AFTER)
                    {
                        if (has_event(core::CatalogManager::TriggerEvent::INSERT))
                        {
                            emit_trigger(2);
                        }
                        if (has_event(core::CatalogManager::TriggerEvent::UPDATE))
                        {
                            emit_trigger(4);
                        }
                        if (has_event(core::CatalogManager::TriggerEvent::DELETE))
                        {
                            emit_trigger(6);
                        }
                    }
                }
            }
            if (tr_first)
            {
                triggers_sql =
                    "SELECT NULL AS RDB$TRIGGER_NAME, NULL AS RDB$RELATION_NAME, NULL AS "
                    "RDB$TRIGGER_SEQUENCE, NULL AS RDB$TRIGGER_TYPE WHERE 1 = 0";
            }
            else
            {
                triggers_sql = tr.str();
            }
        }
        status = ensure_view("RDB$TRIGGERS",
                             triggers_sql,
                             {"RDB$TRIGGER_NAME",
                              "RDB$RELATION_NAME",
                              "RDB$TRIGGER_SEQUENCE",
                              "RDB$TRIGGER_TYPE"},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }

        std::string procedures_sql;
        std::string procedure_params_sql;
        {
            std::ostringstream proc_ss;
            std::ostringstream param_ss;
            bool proc_first = true;
            bool param_first = true;
            for (const auto &proc : procedures)
            {
                uint32_t output_count = 0;
                for (const auto &p : proc.parameters)
                {
                    if (p.mode == core::CatalogManager::ParameterMode::OUT ||
                        p.mode == core::CatalogManager::ParameterMode::INOUT)
                    {
                        ++output_count;
                    }
                }
                if (!proc_first)
                {
                    proc_ss << " UNION ALL ";
                }
                proc_ss << "SELECT '" << escapeLiteral(proc.name)
                        << "' AS RDB$PROCEDURE_NAME, " << output_count
                        << " AS RDB$PROCEDURE_OUTPUTS, NULL AS RDB$OWNER_NAME";
                proc_first = false;

                for (size_t i = 0; i < proc.parameters.size(); ++i)
                {
                    const auto &p = proc.parameters[i];
                    int param_type = 0;
                    switch (p.mode)
                    {
                        case core::CatalogManager::ParameterMode::IN:
                            param_type = 0;
                            break;
                        case core::CatalogManager::ParameterMode::OUT:
                            param_type = 1;
                            break;
                        case core::CatalogManager::ParameterMode::INOUT:
                            param_type = 2;
                            break;
                    }
                    if (!param_first)
                    {
                        param_ss << " UNION ALL ";
                    }
                    const std::string field_source =
                        make_firebird_field_source(proc.name, p.name);
                    param_ss << "SELECT '" << escapeLiteral(proc.name)
                             << "' AS RDB$PROCEDURE_NAME, '" << escapeLiteral(p.name)
                             << "' AS RDB$PARAMETER_NAME, " << param_type
                             << " AS RDB$PARAMETER_TYPE, '" << escapeLiteral(field_source)
                             << "' AS RDB$FIELD_SOURCE, " << static_cast<int>(i)
                             << " AS RDB$PARAMETER_NUMBER, 0 AS RDB$NULL_FLAG";
                    param_first = false;
                }
            }
            if (proc_first)
            {
                procedures_sql =
                    "SELECT NULL AS RDB$PROCEDURE_NAME, NULL AS RDB$PROCEDURE_OUTPUTS, NULL "
                    "AS RDB$OWNER_NAME WHERE 1 = 0";
            }
            else
            {
                procedures_sql = proc_ss.str();
            }
            if (param_first)
            {
                procedure_params_sql =
                    "SELECT NULL AS RDB$PROCEDURE_NAME, NULL AS RDB$PARAMETER_NAME, NULL AS "
                    "RDB$PARAMETER_TYPE, NULL AS RDB$FIELD_SOURCE, NULL AS "
                    "RDB$PARAMETER_NUMBER, NULL AS RDB$NULL_FLAG WHERE 1 = 0";
            }
            else
            {
                procedure_params_sql = param_ss.str();
            }
        }

        status = ensure_view("RDB$PROCEDURES",
                             procedures_sql,
                             {"RDB$PROCEDURE_NAME",
                              "RDB$PROCEDURE_OUTPUTS",
                              "RDB$OWNER_NAME"},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }
        status = ensure_view("RDB$PROCEDURE_PARAMETERS",
                             procedure_params_sql,
                             {"RDB$PROCEDURE_NAME",
                              "RDB$PARAMETER_NAME",
                              "RDB$PARAMETER_TYPE",
                              "RDB$FIELD_SOURCE",
                              "RDB$PARAMETER_NUMBER",
                              "RDB$NULL_FLAG"},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }

        const auto to_lower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return s;
        };

        const auto extract_from_clause = [&](const std::string &definition) {
            std::vector<std::string> result;
            if (definition.empty())
            {
                return result;
            }
            std::string def_lower = to_lower(definition);
            auto pos = def_lower.find(" from ");
            if (pos == std::string::npos)
            {
                return result;
            }
            pos += 6;
            while (pos < def_lower.size())
            {
                while (pos < def_lower.size() &&
                       std::isspace(static_cast<unsigned char>(def_lower[pos])))
                {
                    ++pos;
                }
                if (pos >= def_lower.size())
                {
                    break;
                }
                const size_t start = pos;
                while (pos < def_lower.size())
                {
                    const char c = def_lower[pos];
                    if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ';')
                    {
                        break;
                    }
                    ++pos;
                }
                if (pos > start)
                {
                    result.emplace_back(definition.substr(start, pos - start));
                }
                while (pos < def_lower.size() && def_lower[pos] != ',' && def_lower[pos] != ';')
                {
                    ++pos;
                }
                if (pos < def_lower.size() && (def_lower[pos] == ',' || def_lower[pos] == ';'))
                {
                    ++pos;
                }
                else
                {
                    break;
                }
            }
            return result;
        };

        std::string view_relations_sql;
        {
            std::ostringstream vr;
            bool vr_first = true;
            for (const auto &v : views)
            {
                std::vector<core::CatalogManager::DependencyInfo> deps;
                if (catalog->getDependenciesFor(v.view_id, deps, ctx) != core::Status::OK)
                {
                    deps.clear();
                }
                size_t emitted = 0;
                for (const auto &dep : deps)
                {
                    if (dep.dependent_type != core::CatalogManager::ObjectType::VIEW ||
                        dep.referenced_type != core::CatalogManager::ObjectType::TABLE)
                    {
                        continue;
                    }
                    std::string rel_name;
                    const auto t_it = table_by_id.find(dep.referenced_object_id);
                    if (t_it != table_by_id.end())
                    {
                        rel_name = t_it->second.table_name;
                    }
                    if (!vr_first)
                    {
                        vr << " UNION ALL ";
                    }
                    vr << "SELECT '" << escapeLiteral(v.name) << "' AS RDB$VIEW_NAME, "
                       << (rel_name.empty() ? "NULL" : ("'" + escapeLiteral(rel_name) + "'"))
                       << " AS RDB$RELATION_NAME";
                    vr_first = false;
                    ++emitted;
                }
                if (emitted == 0 && !v.definition.empty())
                {
                    const auto bases = extract_from_clause(v.definition);
                    for (const auto &base : bases)
                    {
                        std::string rel_name;
                        const auto name_key = to_lower(base);
                        const auto by_name = table_by_name.find(name_key);
                        if (by_name != table_by_name.end())
                        {
                            rel_name = by_name->second;
                        }
                        else
                        {
                            rel_name = base;
                        }
                        if (!vr_first)
                        {
                            vr << " UNION ALL ";
                        }
                        vr << "SELECT '" << escapeLiteral(v.name) << "' AS RDB$VIEW_NAME, '"
                           << escapeLiteral(rel_name) << "' AS RDB$RELATION_NAME";
                        vr_first = false;
                        ++emitted;
                    }
                }
                if (emitted == 0)
                {
                    if (!vr_first)
                    {
                        vr << " UNION ALL ";
                    }
                    vr << "SELECT '" << escapeLiteral(v.name)
                       << "' AS RDB$VIEW_NAME, NULL AS RDB$RELATION_NAME";
                    vr_first = false;
                }
            }
            if (vr_first)
            {
                view_relations_sql =
                    "SELECT NULL AS RDB$VIEW_NAME, NULL AS RDB$RELATION_NAME WHERE 1 = 0";
            }
            else
            {
                view_relations_sql = vr.str();
            }
        }
        status = ensure_view("RDB$VIEW_RELATIONS",
                             view_relations_sql,
                             {"RDB$VIEW_NAME", "RDB$RELATION_NAME"},
                             true);
        if (status != core::Status::OK)
        {
            return status;
        }

        clearErrorContextSuccess(ctx);
        return core::Status::OK;
    }

    auto dropFirebirdVirtualCatalog(core::Database *database,
                                    const FirebirdVirtualCatalogRequest &request,
                                    core::ErrorContext *ctx) -> core::Status
    {
        auto status = requireFirebirdEmulationPackage(request.profile_id, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        if (database == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Database not initialized",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        auto *catalog = database->catalog_manager();
        if (catalog == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Catalog manager not available",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        FirebirdVirtualCatalogResponse response{};
        auto binding_status = deriveFirebirdVirtualCatalogBinding(request, response, ctx);
        if (binding_status != core::Status::OK)
        {
            return binding_status;
        }

        core::CatalogManager::SchemaInfo fb_schema;
        status = resolveFirebirdSchema(catalog, response, fb_schema, ctx);
        if (status == core::Status::INVALID_ARGUMENT || status == core::Status::NOT_FOUND)
        {
            clearErrorContextSuccess(ctx);
            return core::Status::OK;
        }
        if (status != core::Status::OK)
        {
            return status;
        }

        std::vector<core::CatalogManager::ViewInfo> views;
        core::ErrorContext list_ctx;
        status = catalog->listViewsForSchema(fb_schema.schema_id, views, &list_ctx);
        if (status != core::Status::OK && status != core::Status::INVALID_ARGUMENT &&
            status != core::Status::NOT_FOUND)
        {
            if (ctx != nullptr && !list_ctx.message.empty())
            {
                ctx->set(list_ctx.code,
                         list_ctx.message.c_str(),
                         list_ctx.file,
                         list_ctx.line,
                         list_ctx.function);
            }
            return status;
        }

        for (const auto &view : views)
        {
            core::ErrorContext drop_view_ctx;
            const auto drop_view_status = catalog->dropView(view.view_id, true, &drop_view_ctx);
            if (drop_view_status != core::Status::OK)
            {
                if (ctx != nullptr && !drop_view_ctx.message.empty())
                {
                    ctx->set(drop_view_ctx.code,
                             drop_view_ctx.message.c_str(),
                             drop_view_ctx.file,
                             drop_view_ctx.line,
                             drop_view_ctx.function);
                }
                return drop_view_status;
            }
        }

        status = catalog->dropSchema(fb_schema.schema_id, true, ctx);
        if (status == core::Status::INVALID_ARGUMENT || status == core::Status::NOT_FOUND)
        {
            clearErrorContextSuccess(ctx);
            return core::Status::OK;
        }
        return status;
    }

} // namespace scratchbird::udr
