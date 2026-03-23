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
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/udr/language_udr_runtime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird::core
{
    class Database;
}

namespace scratchbird::sblr
{

    enum class DialectCompilerPayloadFormat : uint8_t
    {
        SQL_TEXT = 0,
        FIREBIRD_BLR = 1
    };

    struct DialectCompilerSessionEnvelope
    {
        std::string profile_id;
        std::string profile_version = "1.0";
        std::string dialect_tag;
        core::ID current_schema_id{};
        std::string current_schema_name;
        std::vector<std::string> search_path;
        std::string emulated_schema_root;
        core::ID principal_id{};
        core::ID active_role_id{};
        core::ID auth_key_id{};
        uint64_t transaction_id = 1;
        bool engine_dynamic_sql = false;
    };

    struct DialectCompilerRequest
    {
        core::ID request_id{};
        std::string module_name;
        DialectCompilerSessionEnvelope session{};
        DialectCompilerPayloadFormat payload_format = DialectCompilerPayloadFormat::SQL_TEXT;
        std::vector<uint8_t> payload;
        bool optimizations_enabled = true;
        bool stats_enabled = false;
        bool compile_permission_granted = true;
        bool requires_network_access = false;
        bool requires_filesystem_write = false;
        udr::LanguageUdrCompileRequest::SandboxPolicy sandbox_policy{};
        udr::LanguageUdrCompileRequest::ResourceLimits resource_limits{};
    };

    struct DialectCompilerResponse
    {
        bool success = false;
        std::string contract_id;
        std::string profile_id;
        std::string module_name;
        std::string native_feature_key;
        std::vector<uint8_t> bytecode;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct DialectCompilerInstallDescriptor
    {
        std::string profile_id;
        std::string profile_version;
        std::string module_name;
        std::string translation_mode;
        std::vector<std::string> feature_keys;
    };

    auto builtinDialectCompilerInstallDescriptors()
        -> const std::vector<DialectCompilerInstallDescriptor> &;

    auto defaultDialectCompilerRegistry() -> udr::LanguageUdrRegistry &;

    auto compileDialectToSblr(core::Database *db,
                              const DialectCompilerRequest &request,
                              DialectCompilerResponse &response_out,
                              core::ErrorContext *ctx = nullptr,
                              const udr::LanguageUdrRegistry *registry_override = nullptr)
        -> core::Status;

    auto compileFirebirdDialectToSblr(core::Database *db,
                                      const DialectCompilerRequest &request,
                                      DialectCompilerResponse &response_out,
                                      core::ErrorContext *ctx = nullptr)
        -> core::Status;

} // namespace scratchbird::sblr
