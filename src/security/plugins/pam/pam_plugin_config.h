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
namespace pam {

enum class PamPluginConfigStatus : uint8_t {
    OK = 0,
    MISSING_REQUIRED_KEY,
    INVALID_VALUE,
};

struct PamPluginConfig {
    std::string service_name;
    std::vector<std::string> allowed_modules;
    uint32_t conversation_timeout_ms = 2000;
};

PamPluginConfigStatus loadPamPluginConfig(const std::map<std::string, std::string>& values,
                                          PamPluginConfig& out,
                                          std::string* error_out = nullptr);

}  // namespace pam
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
