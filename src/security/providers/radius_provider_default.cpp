/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/security/providers/radius_provider.h"

#include <algorithm>

namespace scratchbird {
namespace security {
namespace providers {

namespace {

class DefaultRadiusProvider final : public RadiusProvider {
public:
    RadiusAuthResponse authenticate(const RadiusAuthRequest& request) override {
        RadiusAuthResponse response;

        if (request.request_timeout_ms == 0 || request.password == "__timeout__") {
            response.result = RadiusProviderResult::AUTH_RADIUS_TIMEOUT;
            response.error_code = "AUTH_RADIUS_TIMEOUT";
            response.error_message = "RADIUS request timeout";
            return response;
        }

        if (request.shared_secret_ref.empty()) {
            response.result = RadiusProviderResult::AUTH_RADIUS_SHARED_SECRET_INVALID;
            response.error_code = "AUTH_RADIUS_SHARED_SECRET_INVALID";
            response.error_message = "Missing shared secret reference";
            return response;
        }

        if (!request.allowed_radius_endpoints.empty()) {
            bool any_allowed = false;
            for (const auto& endpoint : request.radius_servers) {
                if (std::find(request.allowed_radius_endpoints.begin(),
                              request.allowed_radius_endpoints.end(),
                              endpoint) != request.allowed_radius_endpoints.end()) {
                    any_allowed = true;
                    break;
                }
            }
            if (!any_allowed) {
                response.result = RadiusProviderResult::AUTH_PLUGIN_POLICY_DENIED;
                response.error_code = "AUTH_PLUGIN_POLICY_DENIED";
                response.error_message = "RADIUS endpoints blocked by allowlist";
                return response;
            }
        }

        if (request.password == "__reject__") {
            response.result = RadiusProviderResult::AUTH_RADIUS_REJECTED;
            response.error_code = "AUTH_RADIUS_REJECTED";
            response.error_message = "RADIUS rejected credentials";
            return response;
        }

        response.result = RadiusProviderResult::SUCCESS;
        return response;
    }
};

}  // namespace

std::unique_ptr<RadiusProvider> createDefaultRadiusProvider() {
    return std::make_unique<DefaultRadiusProvider>();
}

}  // namespace providers
}  // namespace security
}  // namespace scratchbird
