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
namespace ldap {

enum class LdapPluginConfigStatus : uint8_t {
    OK = 0,
    MISSING_REQUIRED_KEY,
    INVALID_VALUE,
};

enum class LdapRuntimeProfile : uint8_t {
    PRODUCTION = 0,
    TEST = 1,
};

struct LdapPluginConfig {
    std::string ldap_uri;
    std::string bind_dn_template;
    std::string group_role_map;
    uint32_t connect_timeout_ms = 3000;
    bool require_starttls = true;
    LdapRuntimeProfile runtime_profile = LdapRuntimeProfile::PRODUCTION;
    std::vector<std::string> allowed_ldap_endpoints;
};

LdapPluginConfigStatus loadLdapPluginConfig(const std::map<std::string, std::string>& values,
                                            LdapPluginConfig& out,
                                            std::string* error_out = nullptr);

}  // namespace ldap
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
