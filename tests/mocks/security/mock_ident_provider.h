/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include "scratchbird/security/providers/ident_provider.h"

namespace scratchbird {
namespace testing {

class MockIdentProvider final : public security::providers::IdentProvider {
public:
    void setResponse(const security::providers::IdentAuthResponse& response) {
        response_ = response;
    }

    const security::providers::IdentAuthRequest& lastRequest() const {
        return last_request_;
    }

    security::providers::IdentAuthResponse authenticate(
        const security::providers::IdentAuthRequest& request) override {
        last_request_ = request;
        return response_;
    }

private:
    security::providers::IdentAuthRequest last_request_{};
    security::providers::IdentAuthResponse response_{};
};

}  // namespace testing
}  // namespace scratchbird
