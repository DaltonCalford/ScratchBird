/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include "scratchbird/security/providers/kerberos_provider.h"

namespace scratchbird {
namespace testing {

class MockKerberosProvider final : public security::providers::KerberosProvider {
public:
    void setResponse(const security::providers::KerberosAuthResponse& response) {
        response_ = response;
    }

    const security::providers::KerberosAuthRequest& lastRequest() const {
        return last_request_;
    }

    security::providers::KerberosAuthResponse authenticate(
        const security::providers::KerberosAuthRequest& request) override {
        last_request_ = request;
        return response_;
    }

private:
    security::providers::KerberosAuthRequest last_request_{};
    security::providers::KerberosAuthResponse response_{};
};

}  // namespace testing
}  // namespace scratchbird
