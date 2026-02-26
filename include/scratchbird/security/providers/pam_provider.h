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

enum class PamProviderResult : uint8_t {
    SUCCESS = 0,
    AUTH_PAM_DENIED,
    AUTH_PAM_SERVICE_NOT_ALLOWED,
    AUTH_PAM_CONVERSATION_TIMEOUT,
    INTERNAL_ERROR,
};

struct PamAuthRequest {
    std::string username;
    std::string password;
    std::string service_name;
    std::vector<std::string> allowed_modules;
    uint32_t conversation_timeout_ms = 2000;
};

struct PamAuthResponse {
    PamProviderResult result = PamProviderResult::INTERNAL_ERROR;
    std::string error_code;
    std::string error_message;
};

class PamProvider {
public:
    virtual ~PamProvider() = default;
    virtual PamAuthResponse authenticate(const PamAuthRequest& request) = 0;
};

std::unique_ptr<PamProvider> createDefaultPamProvider();

}  // namespace providers
}  // namespace security
}  // namespace scratchbird
