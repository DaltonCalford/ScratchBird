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

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"

#include <string>

namespace scratchbird::core
{
    class Database;
}

namespace scratchbird::udr
{

    struct EmulationCatalogRequest
    {
        std::string profile_id;
        std::string schema_path;
        std::string server_name;
        std::string database_name;
        catalog::ProtocolType protocol = catalog::ProtocolType::SCRATCHBIRD;
    };

    auto ensureEmulatedCatalog(core::Database *database,
                               const EmulationCatalogRequest &request,
                               core::ErrorContext *ctx = nullptr) -> core::Status;

    auto dropEmulatedCatalog(core::Database *database,
                             const EmulationCatalogRequest &request,
                             core::ErrorContext *ctx = nullptr) -> core::Status;

} // namespace scratchbird::udr
