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

#include <string>
#include <vector>

namespace scratchbird::core
{

    struct EmulationPackageScaffold
    {
        std::string profile_id;
        std::string listener_executable_name;
        std::string parser_executable_name;
        std::string parser_package_name;
        std::string compiler_udr_package_name;
        std::string emulation_udr_package_name;
        std::string bundle_contract_id;
        bool supports_sql_text = true;
        bool supports_engine_dynamic_sql = true;
        bool supports_message_blr = false;
        bool supports_executable_blr = false;
    };

    auto builtinEmulationPackageScaffolds()
        -> const std::vector<EmulationPackageScaffold> &;

    auto resolveEmulationPackageScaffold(const std::string &profile_id,
                                         const EmulationPackageScaffold *&scaffold_out,
                                         ErrorContext *ctx = nullptr) -> Status;

} // namespace scratchbird::core
