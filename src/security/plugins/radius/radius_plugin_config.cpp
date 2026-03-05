/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "radius_plugin_config.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace scratchbird {
namespace security {
namespace plugins {
namespace radius {

namespace {

std::string trimAscii(std::string value) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

bool parseUInt32(const std::string& value, uint32_t& out) {
    try {
        out = static_cast<uint32_t>(std::stoul(trimAscii(value)));
        return true;
    } catch (...) {
        return false;
    }
}

bool parseRuntimeProfile(std::string value, RadiusRuntimeProfile& out) {
    value = trimAscii(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (value == "production" || value == "prod") {
        out = RadiusRuntimeProfile::PRODUCTION;
        return true;
    }
    if (value == "test") {
        out = RadiusRuntimeProfile::TEST;
        return true;
    }
    return false;
}

std::vector<std::string> splitCsv(const std::string& value) {
    std::vector<std::string> out;
    std::istringstream input(value);
    std::string token;
    while (std::getline(input, token, ',')) {
        token = trimAscii(std::move(token));
        if (!token.empty()) {
            out.push_back(token);
        }
    }
    return out;
}

}  // namespace

RadiusPluginConfigStatus loadRadiusPluginConfig(const std::map<std::string, std::string>& values,
                                                RadiusPluginConfig& out,
                                                std::string* error_out) {
    auto set_error = [error_out](const std::string& message) {
        if (error_out) {
            *error_out = message;
        }
    };

    const auto servers_it = values.find("radius_servers");
    const auto secret_it = values.find("shared_secret_ref");
    if (servers_it == values.end() || secret_it == values.end()) {
        set_error("Missing required keys: radius_servers/shared_secret_ref");
        return RadiusPluginConfigStatus::MISSING_REQUIRED_KEY;
    }

    out.radius_servers = splitCsv(servers_it->second);
    out.shared_secret_ref = trimAscii(secret_it->second);

    auto timeout_it = values.find("request_timeout_ms");
    if (timeout_it != values.end() && !parseUInt32(timeout_it->second, out.request_timeout_ms)) {
        set_error("request_timeout_ms must be numeric");
        return RadiusPluginConfigStatus::INVALID_VALUE;
    }

    auto profile_it = values.find("runtime_profile");
    if (profile_it != values.end() &&
        !parseRuntimeProfile(profile_it->second, out.runtime_profile)) {
        set_error("runtime_profile must be one of: production, test");
        return RadiusPluginConfigStatus::INVALID_VALUE;
    }

    auto allowlist_it = values.find("allowed_radius_endpoints");
    if (allowlist_it != values.end()) {
        out.allowed_radius_endpoints = splitCsv(allowlist_it->second);
    }

    return RadiusPluginConfigStatus::OK;
}

}  // namespace radius
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
