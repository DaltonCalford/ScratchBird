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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird {
namespace security {
namespace providers {

enum class LdapProviderResult : uint8_t {
    SUCCESS = 0,
    AUTH_LDAP_BIND_FAILED,
    AUTH_LDAP_TIMEOUT,
    AUTH_PLUGIN_POLICY_DENIED,
    AUTH_LDAP_CONFIG_INVALID,
    AUTH_LDAP_TRANSPORT_ERROR,
    INTERNAL_ERROR,
};

struct LdapAuthRequest {
    std::string username;
    std::string password;
    std::string client_address;

    std::string ldap_uri;
    std::string bind_dn_template;
    std::string group_role_map;
    uint32_t connect_timeout_ms = 3000;
    bool require_starttls = true;
    std::vector<std::string> allowed_ldap_endpoints;
};

struct LdapAuthResponse {
    LdapProviderResult result = LdapProviderResult::INTERNAL_ERROR;
    std::string error_code;
    std::string error_message;
    std::vector<std::string> mapped_roles;
    std::string resolved_principal;
};

class LdapProvider {
public:
    virtual ~LdapProvider() = default;
    virtual LdapAuthResponse authenticate(const LdapAuthRequest& request) = 0;
};

std::unique_ptr<LdapProvider> createDefaultLdapProvider();

}  // namespace providers
}  // namespace security
}  // namespace scratchbird
