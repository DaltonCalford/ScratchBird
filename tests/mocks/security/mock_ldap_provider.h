/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include "scratchbird/security/providers/ldap_provider.h"

namespace scratchbird {
namespace testing {

class MockLdapProvider final : public security::providers::LdapProvider {
public:
    void setResponse(const security::providers::LdapAuthResponse& response) {
        response_ = response;
    }

    const security::providers::LdapAuthRequest& lastRequest() const {
        return last_request_;
    }

    security::providers::LdapAuthResponse authenticate(
        const security::providers::LdapAuthRequest& request) override {
        last_request_ = request;
        return response_;
    }

private:
    security::providers::LdapAuthRequest last_request_{};
    security::providers::LdapAuthResponse response_{};
};

}  // namespace testing
}  // namespace scratchbird
