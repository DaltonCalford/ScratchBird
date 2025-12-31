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
