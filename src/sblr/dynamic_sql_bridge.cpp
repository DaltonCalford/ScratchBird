/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/dynamic_sql_bridge.h"

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/emulation_package_manifest.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/udr/dialect_compiler_udr.h"

#include <algorithm>
#include <cctype>

namespace scratchbird::sblr
{

    namespace
    {
        struct DynamicSqlDialectTarget
        {
            std::string profile_id;
            std::string module_name;
            std::string dialect_tag;
        };

        auto toLowerAscii(std::string value) -> std::string
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        auto resolveDialectTarget(const core::ConnectionContext *conn_ctx,
                                  DynamicSqlDialectTarget &target) -> bool
        {
            if (conn_ctx == nullptr)
            {
                return false;
            }

            std::string dialect = toLowerAscii(conn_ctx->dialect_tag());
            if (dialect.empty())
            {
                const std::string current_schema = toLowerAscii(conn_ctx->current_schema());
                if (current_schema.find("firebird") != std::string::npos)
                {
                    dialect = "firebird";
                }
                else if (current_schema.find("postgresql") != std::string::npos)
                {
                    dialect = "postgresql";
                }
                else if (current_schema.find("mysql") != std::string::npos)
                {
                    dialect = "mysql";
                }
            }

            if (dialect == "firebird" || dialect == "firebirdsql" ||
                dialect == "firebird_emulation")
            {
                target.profile_id = "firebirdsql";
                target.module_name = "firebird_emulation";
                target.dialect_tag = "firebird";
                return true;
            }
            if (dialect == "postgresql" || dialect == "postgresql_emulation" ||
                dialect == "postgres")
            {
                target.profile_id = "postgresql";
                target.module_name = "postgresql_emulation";
                target.dialect_tag = "postgresql";
                return true;
            }
            if (dialect == "mysql" || dialect == "mysql_emulation")
            {
                target.profile_id = "mysql";
                target.module_name = "mysql_emulation";
                target.dialect_tag = "mysql";
                return true;
            }
            if (dialect == "scratchbird" || dialect == "scratchbird_v3" ||
                dialect == "scratchbird_native" || dialect == "native")
            {
                target.profile_id = "scratchbird";
                target.module_name = "scratchbird_native";
                target.dialect_tag = "scratchbird";
                return true;
            }

            target.profile_id = "scratchbird";
            target.module_name = "scratchbird_native";
            target.dialect_tag = "scratchbird";
            return true;
        }
    } // namespace

    auto compileEngineDynamicSql(core::Database *db,
                                 core::ConnectionContext *conn_ctx,
                                 const std::string &sql_text,
                                 DynamicSqlCompileResponse &response_out,
                                 core::ErrorContext *ctx) -> core::Status
    {
        response_out = DynamicSqlCompileResponse{};

        if (db == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Dynamic SQL bridge requires a database",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }
        if (conn_ctx == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Dynamic SQL bridge requires a connection context",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }
        if (sql_text.empty())
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Dynamic SQL bridge requires a non-empty SQL statement",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        DynamicSqlDialectTarget target{};
        if (!resolveDialectTarget(conn_ctx, target))
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::NOT_SUPPORTED,
                         "No installed dynamic SQL compiler UDR is registered for the current dialect",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::NOT_SUPPORTED;
        }

        core::EmulationPackageBundle package_bundle{};
        core::EmulationPackageRequirement package_requirement{};
        package_requirement.compiler_udr = true;
        auto package_status = core::requireEmulationPackageBundle(target.profile_id,
                                                                  package_requirement,
                                                                  package_bundle,
                                                                  ctx);
        if (package_status != core::Status::OK)
        {
            return package_status;
        }

        DialectCompilerRequest request{};
        request.request_id = core::generateUuidV7();
        request.module_name = target.module_name;
        request.session.profile_id = target.profile_id;
        request.session.dialect_tag = conn_ctx->dialect_tag().empty()
                                          ? target.dialect_tag
                                          : conn_ctx->dialect_tag();
        request.session.current_schema_id = conn_ctx->getCurrentSchemaId();
        request.session.current_schema_name = conn_ctx->current_schema();
        request.session.search_path = conn_ctx->search_path();
        request.session.emulated_schema_root = conn_ctx->current_schema();
        request.session.principal_id = conn_ctx->getCurrentUserId();
        request.session.active_role_id = conn_ctx->getActiveRoleId();
        request.session.auth_key_id = conn_ctx->authKeyId();
        request.session.transaction_id = std::max<uint64_t>(1, conn_ctx->getCurrentXid());
        request.session.engine_dynamic_sql = true;
        request.payload.assign(sql_text.begin(), sql_text.end());

        DialectCompilerResponse compiler_response{};
        auto status = compileDialectToSblr(db, request, compiler_response, ctx);
        response_out.success = compiler_response.success;
        response_out.profile_id = compiler_response.profile_id.empty()
                                      ? target.profile_id
                                      : compiler_response.profile_id;
        response_out.module_name = compiler_response.module_name.empty()
                                       ? target.module_name
                                       : compiler_response.module_name;
        response_out.bytecode = std::move(compiler_response.bytecode);
        response_out.warnings = std::move(compiler_response.warnings);
        response_out.errors = std::move(compiler_response.errors);
        if (status != core::Status::OK)
        {
            if (response_out.errors.empty() && ctx != nullptr && !ctx->message.empty())
            {
                response_out.errors.push_back(ctx->message);
            }
            return status;
        }
        if (!response_out.success)
        {
            if (response_out.errors.empty() && ctx != nullptr && !ctx->message.empty())
            {
                response_out.errors.push_back(ctx->message);
            }
            return core::Status::INVALID_ARGUMENT;
        }
        return core::Status::OK;
    }

    auto persistViewExecutionMetadataFromSql(core::Database *db,
                                             const core::ID &schema_id,
                                             const std::string &view_name,
                                             const std::string &sql_text,
                                             core::ErrorContext *ctx) -> core::Status
    {
        if (db == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Persisting compiled view metadata requires a database",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }
        if (view_name.empty() || sql_text.empty())
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Persisting compiled view metadata requires a view name and SQL text",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        core::ConnectionContext *conn_ctx = core::ConnectionContext::getCurrent();
        if (conn_ctx == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Persisting compiled view metadata requires an active connection context",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        DynamicSqlCompileResponse compile_response{};
        auto status = compileEngineDynamicSql(db, conn_ctx, sql_text, compile_response, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        core::CatalogManager *catalog = db->catalog_manager();
        if (catalog == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Catalog manager not available while persisting view metadata",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        core::CatalogManager::ViewInfo view_info;
        status = catalog->getView(schema_id, view_name, view_info, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        const std::vector<core::CatalogManager::ViewColumnBinding> empty_insert_bindings;
        return catalog->setViewExecutionMetadata(view_info.view_id,
                                                 compile_response.bytecode,
                                                 empty_insert_bindings,
                                                 compile_response.profile_id,
                                                 {},
                                                 ctx);
    }

} // namespace scratchbird::sblr
