/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/udr/emulation_catalog_udr.h"

#include "scratchbird/catalog/emulation_view_generator.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/emulation_package_manifest.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/udr/dialect_compiler_udr.h"
#include "scratchbird/udr/firebird_emulation_udr.h"

namespace scratchbird::catalog
{

    auto EmulationViewGenerator::compileEmulatedViewQuery(
        const ID &schema_id,
        const std::string &schema_path,
        ProtocolType protocol,
        const std::string &query,
        std::vector<uint8_t> &bytecode_out,
        ErrorContext *ctx) -> Status
    {
        bytecode_out.clear();

        if (catalog_ == nullptr || catalog_->database() == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->set(Status::INVALID_ARGUMENT,
                         "Catalog database is not available for emulated view compilation",
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return Status::INVALID_ARGUMENT;
        }

        scratchbird::sblr::QueryCompilerV3 compiler(catalog_->database());
        compiler.setCurrentSchemaId(schema_id);
        auto result = compiler.compile(query);
        if (!result.success())
        {
            const std::string message = !result.errors().empty()
                ? result.errors().front()
                : "Canonical emulation view compilation did not return SBLR";
            if (ctx != nullptr && ctx->message.empty())
            {
                ctx->set(Status::INVALID_ARGUMENT,
                         message.c_str(),
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return Status::INVALID_ARGUMENT;
        }

        bytecode_out = result.bytecode();
        return Status::OK;
    }

} // namespace scratchbird::catalog

namespace scratchbird::udr
{

    namespace
    {
        auto resolveEmulationProfileId(const EmulationCatalogRequest &request) -> std::string
        {
            if (!request.profile_id.empty())
            {
                return request.profile_id;
            }
            return catalog::protocolTypeToString(request.protocol);
        }

        auto requireEmulationCatalogPackage(const EmulationCatalogRequest &request,
                                            core::ErrorContext *ctx) -> core::Status
        {
            const core::EmulationPackageManifest *manifest = nullptr;
            return core::resolveInstalledEmulationPackage(resolveEmulationProfileId(request),
                                                          core::EmulationPackageKind::EMULATION_UDR,
                                                          manifest,
                                                          ctx);
        }

        auto validateCatalogRequest(core::Database *database,
                                    const EmulationCatalogRequest &request,
                                    core::CatalogManager *&catalog_out,
                                    core::ErrorContext *ctx) -> core::Status
        {
            catalog_out = nullptr;

            auto status = requireEmulationCatalogPackage(request, ctx);
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

            catalog_out = database->catalog_manager();
            if (catalog_out == nullptr)
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

            if (request.protocol == catalog::ProtocolType::SCRATCHBIRD)
            {
                if (ctx != nullptr)
                {
                    ctx->set(core::Status::INVALID_ARGUMENT,
                             "Native ScratchBird catalog routing does not use emulation catalog UDRs",
                             __FILE__,
                             __LINE__,
                             __func__);
                }
                return core::Status::INVALID_ARGUMENT;
            }

            if (request.schema_path.empty() || request.server_name.empty() ||
                request.database_name.empty())
            {
                if (ctx != nullptr)
                {
                    ctx->set(core::Status::INVALID_ARGUMENT,
                             "Emulation catalog request is missing schema path, server, or database name",
                             __FILE__,
                             __LINE__,
                             __func__);
                }
                return core::Status::INVALID_ARGUMENT;
            }

            return core::Status::OK;
        }

        auto makeFirebirdCatalogRequest(const EmulationCatalogRequest &request)
            -> FirebirdVirtualCatalogRequest
        {
            FirebirdVirtualCatalogRequest fb_request{};
            fb_request.profile_id = resolveEmulationProfileId(request);
            fb_request.schema_name = request.schema_path;
            fb_request.server_name = request.server_name;
            fb_request.database_name = request.database_name;
            return fb_request;
        }
    } // namespace

    auto ensureEmulatedCatalog(core::Database *database,
                               const EmulationCatalogRequest &request,
                               core::ErrorContext *ctx) -> core::Status
    {
        core::CatalogManager *catalog = nullptr;
        auto status = validateCatalogRequest(database, request, catalog, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        if (request.protocol == catalog::ProtocolType::FIREBIRD)
        {
            FirebirdVirtualCatalogResponse firebird_response{};
            return ensureFirebirdVirtualCatalog(database,
                                                makeFirebirdCatalogRequest(request),
                                                firebird_response,
                                                ctx);
        }

        catalog::EmulationViewGenerator generator(catalog);
        return generator.generateEmulatedViews(request.schema_path,
                                               request.server_name,
                                               request.database_name,
                                               request.protocol,
                                               ctx);
    }

    auto dropEmulatedCatalog(core::Database *database,
                             const EmulationCatalogRequest &request,
                             core::ErrorContext *ctx) -> core::Status
    {
        core::CatalogManager *catalog = nullptr;
        auto status = validateCatalogRequest(database, request, catalog, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        if (request.protocol == catalog::ProtocolType::FIREBIRD)
        {
            return dropFirebirdVirtualCatalog(database, makeFirebirdCatalogRequest(request), ctx);
        }

        catalog::EmulationViewGenerator generator(catalog);
        return generator.dropEmulatedViews(request.schema_path,
                                           request.server_name,
                                           request.database_name,
                                           request.protocol,
                                           ctx);
    }

} // namespace scratchbird::udr
