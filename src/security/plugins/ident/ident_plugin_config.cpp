/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "ident_plugin_config.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace scratchbird {
namespace security {
namespace plugins {
namespace ident {

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

bool parseBool(std::string value, bool& out) {
    value = trimAscii(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    if (value == "1" || value == "TRUE" || value == "YES" || value == "ON") {
        out = true;
        return true;
    }
    if (value == "0" || value == "FALSE" || value == "NO" || value == "OFF") {
        out = false;
        return true;
    }
    return false;
}

bool parseRuntimeProfile(std::string value, IdentRuntimeProfile& out) {
    value = trimAscii(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (value == "production" || value == "prod") {
        out = IdentRuntimeProfile::PRODUCTION;
        return true;
    }
    if (value == "test") {
        out = IdentRuntimeProfile::TEST;
        return true;
    }
    return false;
}

bool parseUInt32(const std::string& value, uint32_t& out) {
    try {
        out = static_cast<uint32_t>(std::stoul(trimAscii(value)));
        return true;
    } catch (...) {
        return false;
    }
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

IdentPluginConfigStatus loadIdentPluginConfig(const std::map<std::string, std::string>& values,
                                              IdentPluginConfig& out,
                                              std::string* error_out) {
    auto set_error = [error_out](const std::string& message) {
        if (error_out) {
            *error_out = message;
        }
    };

    const auto cidr_it = values.find("trusted_cidrs");
    if (cidr_it == values.end()) {
        set_error("Missing required key: trusted_cidrs");
        return IdentPluginConfigStatus::MISSING_REQUIRED_KEY;
    }

    out.trusted_cidrs = splitCsv(cidr_it->second);

    auto timeout_it = values.find("ident_timeout_ms");
    if (timeout_it != values.end() && !parseUInt32(timeout_it->second, out.ident_timeout_ms)) {
        set_error("ident_timeout_ms must be numeric");
        return IdentPluginConfigStatus::INVALID_VALUE;
    }

    auto require_match_it = values.find("require_username_match");
    if (require_match_it != values.end() &&
        !parseBool(require_match_it->second, out.require_username_match)) {
        set_error("require_username_match must be boolean");
        return IdentPluginConfigStatus::INVALID_VALUE;
    }

    auto profile_it = values.find("runtime_profile");
    if (profile_it != values.end() &&
        !parseRuntimeProfile(profile_it->second, out.runtime_profile)) {
        set_error("runtime_profile must be one of: production, test");
        return IdentPluginConfigStatus::INVALID_VALUE;
    }

    return IdentPluginConfigStatus::OK;
}

}  // namespace ident
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
