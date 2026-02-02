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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "gtest/gtest.h"

inline void EnsureUser(scratchbird::core::CatalogManager* catalog,
                       const std::string& username,
                       const scratchbird::core::ID& default_schema_id = scratchbird::core::ID{},
                       bool is_superuser = false)
{
    scratchbird::core::ErrorContext ctx;
    scratchbird::core::ID user_id;
    scratchbird::core::Status status = catalog->createUser(
        username, "", default_schema_id, is_superuser, user_id, &ctx);
    if (status != scratchbird::core::Status::OK &&
        status != scratchbird::core::Status::FILE_EXISTS)
    {
        FAIL() << "Failed to create user '" << username << "': " << ctx.message;
    }
}
