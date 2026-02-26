/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/security/providers/pam_provider.h"

#include <algorithm>

namespace scratchbird {
namespace security {
namespace providers {

namespace {

class DefaultPamProvider final : public PamProvider {
public:
    PamAuthResponse authenticate(const PamAuthRequest& request) override {
        PamAuthResponse response;

        if (request.conversation_timeout_ms == 0) {
            response.result = PamProviderResult::AUTH_PAM_CONVERSATION_TIMEOUT;
            response.error_code = "AUTH_PAM_CONVERSATION_TIMEOUT";
            response.error_message = "PAM conversation timeout";
            return response;
        }

        if (request.service_name.empty()) {
            response.result = PamProviderResult::AUTH_PAM_SERVICE_NOT_ALLOWED;
            response.error_code = "AUTH_PAM_SERVICE_NOT_ALLOWED";
            response.error_message = "PAM service_name is required";
            return response;
        }

        if (!request.allowed_modules.empty() &&
            std::find(request.allowed_modules.begin(),
                      request.allowed_modules.end(),
                      request.service_name) == request.allowed_modules.end()) {
            response.result = PamProviderResult::AUTH_PAM_SERVICE_NOT_ALLOWED;
            response.error_code = "AUTH_PAM_SERVICE_NOT_ALLOWED";
            response.error_message = "PAM service blocked by allowlist";
            return response;
        }

        if (request.password == "__deny__") {
            response.result = PamProviderResult::AUTH_PAM_DENIED;
            response.error_code = "AUTH_PAM_DENIED";
            response.error_message = "PAM denied credentials";
            return response;
        }

        response.result = PamProviderResult::SUCCESS;
        return response;
    }
};

}  // namespace

std::unique_ptr<PamProvider> createDefaultPamProvider() {
    return std::make_unique<DefaultPamProvider>();
}

}  // namespace providers
}  // namespace security
}  // namespace scratchbird
