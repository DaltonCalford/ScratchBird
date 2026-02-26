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

enum class RadiusProviderResult : uint8_t {
    SUCCESS = 0,
    AUTH_RADIUS_REJECTED,
    AUTH_RADIUS_TIMEOUT,
    AUTH_RADIUS_SHARED_SECRET_INVALID,
    AUTH_PLUGIN_POLICY_DENIED,
    INTERNAL_ERROR,
};

struct RadiusAuthRequest {
    std::string username;
    std::string password;
    std::vector<std::string> radius_servers;
    std::string shared_secret_ref;
    uint32_t request_timeout_ms = 2000;
    std::vector<std::string> allowed_radius_endpoints;
};

struct RadiusAuthResponse {
    RadiusProviderResult result = RadiusProviderResult::INTERNAL_ERROR;
    std::string error_code;
    std::string error_message;
};

class RadiusProvider {
public:
    virtual ~RadiusProvider() = default;
    virtual RadiusAuthResponse authenticate(const RadiusAuthRequest& request) = 0;
};

std::unique_ptr<RadiusProvider> createDefaultRadiusProvider();

}  // namespace providers
}  // namespace security
}  // namespace scratchbird
