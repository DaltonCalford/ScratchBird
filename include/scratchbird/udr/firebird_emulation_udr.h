/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/types.h"

#include <cstdint>
#include <string>

namespace scratchbird::core
{
    class Database;
}

namespace scratchbird::udr
{

    enum class FirebirdEmulationLifecycleOperation : uint8_t
    {
        CREATE_DATABASE = 0,
        DROP_DATABASE = 1
    };

    struct FirebirdSchemaBindingRequest
    {
        std::string profile_id = "firebirdsql";
        std::string database_binding;
    };

    struct FirebirdSchemaBindingResponse
    {
        std::string schema_name;
        std::string legacy_schema_name;
        std::string server_name;
        std::string database_name;
    };

    struct FirebirdVirtualCatalogRequest
    {
        std::string profile_id = "firebirdsql";
        std::string database_binding;
        std::string schema_name;
        std::string server_name;
        std::string database_name;
    };

    struct FirebirdVirtualCatalogResponse : FirebirdSchemaBindingResponse
    {
        core::ID schema_id;
    };

    struct FirebirdLifecycleSqlRequest
    {
        std::string profile_id = "firebirdsql";
        FirebirdEmulationLifecycleOperation operation =
            FirebirdEmulationLifecycleOperation::CREATE_DATABASE;
        std::string database_spec;
    };

    struct FirebirdLifecycleSqlResponse
    {
        std::string sql;
    };

    auto deriveFirebirdSchemaBinding(const FirebirdSchemaBindingRequest &request,
                                     FirebirdSchemaBindingResponse &response,
                                     core::ErrorContext *ctx = nullptr) -> core::Status;

    auto ensureFirebirdVirtualCatalog(core::Database *database,
                                      const FirebirdVirtualCatalogRequest &request,
                                      FirebirdVirtualCatalogResponse &response,
                                      core::ErrorContext *ctx = nullptr) -> core::Status;

    auto dropFirebirdVirtualCatalog(core::Database *database,
                                    const FirebirdVirtualCatalogRequest &request,
                                    core::ErrorContext *ctx = nullptr) -> core::Status;

    auto renderFirebirdLifecycleSql(const FirebirdLifecycleSqlRequest &request,
                                    FirebirdLifecycleSqlResponse &response,
                                    core::ErrorContext *ctx = nullptr) -> core::Status;

} // namespace scratchbird::udr
