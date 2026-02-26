/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include "scratchbird/security/providers/radius_provider.h"

namespace scratchbird {
namespace testing {

class MockRadiusProvider final : public security::providers::RadiusProvider {
public:
    void setResponse(const security::providers::RadiusAuthResponse& response) {
        response_ = response;
    }

    const security::providers::RadiusAuthRequest& lastRequest() const {
        return last_request_;
    }

    security::providers::RadiusAuthResponse authenticate(
        const security::providers::RadiusAuthRequest& request) override {
        last_request_ = request;
        return response_;
    }

private:
    security::providers::RadiusAuthRequest last_request_{};
    security::providers::RadiusAuthResponse response_{};
};

}  // namespace testing
}  // namespace scratchbird
