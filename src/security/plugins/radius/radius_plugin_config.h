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
namespace radius {

enum class RadiusPluginConfigStatus : uint8_t {
    OK = 0,
    MISSING_REQUIRED_KEY,
    INVALID_VALUE,
};

struct RadiusPluginConfig {
    std::vector<std::string> radius_servers;
    std::string shared_secret_ref;
    uint32_t request_timeout_ms = 2000;
    std::vector<std::string> allowed_radius_endpoints;
};

RadiusPluginConfigStatus loadRadiusPluginConfig(const std::map<std::string, std::string>& values,
                                                RadiusPluginConfig& out,
                                                std::string* error_out = nullptr);

}  // namespace radius
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
