/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "kerberos_plugin_config.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace scratchbird {
namespace security {
namespace plugins {
namespace kerberos {

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

bool parseRuntimeProfile(std::string value, KerberosRuntimeProfile& out) {
    value = trimAscii(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (value == "production" || value == "prod") {
        out = KerberosRuntimeProfile::PRODUCTION;
        return true;
    }
    if (value == "test") {
        out = KerberosRuntimeProfile::TEST;
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

KerberosPluginConfigStatus loadKerberosPluginConfig(
    const std::map<std::string, std::string>& values,
    KerberosPluginConfig& out,
    std::string* error_out) {
    auto set_error = [error_out](const std::string& message) {
        if (error_out) {
            *error_out = message;
        }
    };

    const auto spn_it = values.find("service_principal");
    const auto keytab_it = values.find("keytab_path");
    if (spn_it == values.end() || keytab_it == values.end()) {
        set_error("Missing one of required keys: service_principal, keytab_path");
        return KerberosPluginConfigStatus::MISSING_REQUIRED_KEY;
    }

    out.service_principal = trimAscii(spn_it->second);
    out.keytab_path = trimAscii(keytab_it->second);

    auto delegation_it = values.find("allow_delegation");
    if (delegation_it != values.end() && !parseBool(delegation_it->second, out.allow_delegation)) {
        set_error("allow_delegation must be boolean");
        return KerberosPluginConfigStatus::INVALID_VALUE;
    }

    auto replay_it = values.find("max_replay_window_ms");
    if (replay_it != values.end() && !parseUInt32(replay_it->second, out.max_replay_window_ms)) {
        set_error("max_replay_window_ms must be numeric");
        return KerberosPluginConfigStatus::INVALID_VALUE;
    }

    auto profile_it = values.find("runtime_profile");
    if (profile_it != values.end() &&
        !parseRuntimeProfile(profile_it->second, out.runtime_profile)) {
        set_error("runtime_profile must be one of: production, test");
        return KerberosPluginConfigStatus::INVALID_VALUE;
    }

    auto allowlist_it = values.find("allowed_kdc_endpoints");
    if (allowlist_it != values.end()) {
        out.allowed_kdc_endpoints = splitCsv(allowlist_it->second);
    }

    return KerberosPluginConfigStatus::OK;
}

}  // namespace kerberos
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
