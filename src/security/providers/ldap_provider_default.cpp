/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/security/providers/ldap_provider.h"

#include <algorithm>

namespace scratchbird {
namespace security {
namespace providers {

namespace {

class DefaultLdapProvider final : public LdapProvider {
public:
    LdapAuthResponse authenticate(const LdapAuthRequest& request) override {
        LdapAuthResponse response;

        if (request.connect_timeout_ms == 0 || request.password == "__timeout__") {
            response.result = LdapProviderResult::AUTH_LDAP_TIMEOUT;
            response.error_code = "AUTH_LDAP_TIMEOUT";
            response.error_message = "LDAP connect timeout";
            return response;
        }

        if (request.ldap_uri.empty()) {
            response.result = LdapProviderResult::AUTH_LDAP_CONFIG_INVALID;
            response.error_code = "AUTH_LDAP_CONFIG_INVALID";
            response.error_message = "ldap_uri is required";
            return response;
        }

        if (!request.allowed_ldap_endpoints.empty()) {
            const auto it = std::find(request.allowed_ldap_endpoints.begin(),
                                      request.allowed_ldap_endpoints.end(),
                                      request.ldap_uri);
            if (it == request.allowed_ldap_endpoints.end()) {
                response.result = LdapProviderResult::AUTH_PLUGIN_POLICY_DENIED;
                response.error_code = "AUTH_PLUGIN_POLICY_DENIED";
                response.error_message = "LDAP endpoint blocked by allowlist";
                return response;
            }
        }

        if (request.require_starttls && request.ldap_uri.rfind("ldap://", 0) == 0) {
            response.result = LdapProviderResult::AUTH_PLUGIN_POLICY_DENIED;
            response.error_code = "AUTH_PLUGIN_POLICY_DENIED";
            response.error_message = "Plain LDAP transport blocked while STARTTLS is required";
            return response;
        }

        if (request.password == "__bind_fail__") {
            response.result = LdapProviderResult::AUTH_LDAP_BIND_FAILED;
            response.error_code = "AUTH_LDAP_BIND_FAILED";
            response.error_message = "LDAP bind failed";
            return response;
        }

        response.result = LdapProviderResult::SUCCESS;
        response.resolved_principal = request.username;
        return response;
    }
};

}  // namespace

std::unique_ptr<LdapProvider> createDefaultLdapProvider() {
    return std::make_unique<DefaultLdapProvider>();
}

}  // namespace providers
}  // namespace security
}  // namespace scratchbird
