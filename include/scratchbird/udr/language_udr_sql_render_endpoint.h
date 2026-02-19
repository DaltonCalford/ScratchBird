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
#include "scratchbird/sblr/v3_types.h"
#include "scratchbird/udr/language_udr_runtime.h"
#include "scratchbird/udr/language_udr_test_harness.h"

#include <string>
#include <vector>

namespace scratchbird::udr
{

    struct LanguageUdrSblrSqlRenderRequest
    {
        core::ID request_id;
        std::string profile_id;
        std::string profile_version = "1.0";
        std::string native_feature_key;
        core::ID principal_id;
        std::string role_context_signature;
        bool render_permission_granted = true;
        scratchbird::sblr::v3::Instruction root_instruction{};
    };

    struct LanguageUdrSblrSqlRenderResponse
    {
        core::ID request_id;
        bool success = false;
        std::string native_feature_key;
        std::string sql_text;
        std::string contract_id;
        std::string canonical_opcode_symbol;
        std::vector<LanguageUdrDiagnostic> diagnostics;
    };

    auto renderSblrToNativeSqlEndpoint(const LanguageUdrRegistry &registry,
                                       const LanguageUdrSblrSqlRenderRequest &request,
                                       LanguageUdrSblrSqlRenderResponse &response_out,
                                       core::ErrorContext *ctx = nullptr) -> core::Status;

} // namespace scratchbird::udr
