/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include "scratchbird/security/providers/pam_provider.h"

namespace scratchbird {
namespace testing {

class MockPamProvider final : public security::providers::PamProvider {
public:
    void setResponse(const security::providers::PamAuthResponse& response) {
        response_ = response;
    }

    const security::providers::PamAuthRequest& lastRequest() const {
        return last_request_;
    }

    security::providers::PamAuthResponse authenticate(
        const security::providers::PamAuthRequest& request) override {
        last_request_ = request;
        return response_;
    }

private:
    security::providers::PamAuthRequest last_request_{};
    security::providers::PamAuthResponse response_{};
};

}  // namespace testing
}  // namespace scratchbird
