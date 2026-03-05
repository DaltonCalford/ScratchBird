/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace scratchbird {
namespace security {
namespace plugins {
namespace ident {

enum class IdentPluginConfigStatus : uint8_t {
    OK = 0,
    MISSING_REQUIRED_KEY,
    INVALID_VALUE,
};

enum class IdentRuntimeProfile : uint8_t {
    PRODUCTION = 0,
    TEST = 1,
};

struct IdentPluginConfig {
    uint32_t ident_timeout_ms = 1000;
    std::vector<std::string> trusted_cidrs;
    bool require_username_match = true;
    IdentRuntimeProfile runtime_profile = IdentRuntimeProfile::PRODUCTION;
};

IdentPluginConfigStatus loadIdentPluginConfig(const std::map<std::string, std::string>& values,
                                              IdentPluginConfig& out,
                                              std::string* error_out = nullptr);

}  // namespace ident
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
