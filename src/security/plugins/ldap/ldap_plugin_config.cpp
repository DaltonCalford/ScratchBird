/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "ldap_plugin_config.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace scratchbird {
namespace security {
namespace plugins {
namespace ldap {

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

bool parseUInt32(const std::string& value, uint32_t& out) {
    try {
        const unsigned long parsed = std::stoul(trimAscii(value));
        out = static_cast<uint32_t>(parsed);
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

LdapPluginConfigStatus loadLdapPluginConfig(const std::map<std::string, std::string>& values,
                                            LdapPluginConfig& out,
                                            std::string* error_out) {
    auto set_error = [error_out](const std::string& message) {
        if (error_out) {
            *error_out = message;
        }
    };

    const auto uri_it = values.find("ldap_uri");
    const auto bind_it = values.find("bind_dn_template");
    const auto map_it = values.find("group_role_map");
    if (uri_it == values.end() || bind_it == values.end() || map_it == values.end()) {
        set_error("Missing one of required keys: ldap_uri, bind_dn_template, group_role_map");
        return LdapPluginConfigStatus::MISSING_REQUIRED_KEY;
    }

    out.ldap_uri = trimAscii(uri_it->second);
    out.bind_dn_template = trimAscii(bind_it->second);
    out.group_role_map = trimAscii(map_it->second);

    auto timeout_it = values.find("connect_timeout_ms");
    if (timeout_it != values.end() && !parseUInt32(timeout_it->second, out.connect_timeout_ms)) {
        set_error("connect_timeout_ms must be numeric");
        return LdapPluginConfigStatus::INVALID_VALUE;
    }

    auto tls_it = values.find("require_starttls");
    if (tls_it != values.end() && !parseBool(tls_it->second, out.require_starttls)) {
        set_error("require_starttls must be boolean");
        return LdapPluginConfigStatus::INVALID_VALUE;
    }

    auto allowlist_it = values.find("allowed_ldap_endpoints");
    if (allowlist_it != values.end()) {
        out.allowed_ldap_endpoints = splitCsv(allowlist_it->second);
    }

    return LdapPluginConfigStatus::OK;
}

}  // namespace ldap
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
