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

#include <string>
#include <vector>

namespace scratchbird::core
{
    class Database;
    class ConnectionContext;
}

namespace scratchbird::sblr
{

    struct DynamicSqlCompileResponse
    {
        bool success = false;
        std::string profile_id;
        std::string module_name;
        std::vector<uint8_t> bytecode;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    auto compileEngineDynamicSql(core::Database *db,
                                 core::ConnectionContext *conn_ctx,
                                 const std::string &sql_text,
                                 DynamicSqlCompileResponse &response_out,
                                 core::ErrorContext *ctx = nullptr) -> core::Status;

    auto persistViewExecutionMetadataFromSql(core::Database *db,
                                             const core::ID &schema_id,
                                             const std::string &view_name,
                                             const std::string &sql_text,
                                             core::ErrorContext *ctx = nullptr)
        -> core::Status;

} // namespace scratchbird::sblr
