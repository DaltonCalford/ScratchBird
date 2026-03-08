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

#include <string>

namespace scratchbird::sblr::jit
{
    struct LlvmToolchainInfo
    {
        bool available = false;
        std::string provider_identity;
        std::string provider_version;
        std::string host_target_triple;
    };

    auto llvmToolchainInfo() -> const LlvmToolchainInfo&;
    auto llvmToolchainAvailable() -> bool;
    auto normalizeLlvmTargetTriple(const std::string& requested) -> std::string;
}
