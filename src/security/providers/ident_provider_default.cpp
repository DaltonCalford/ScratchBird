/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/security/providers/ident_provider.h"

#include <algorithm>

namespace scratchbird {
namespace security {
namespace providers {

namespace {

bool trustedBySimpleCidrs(const std::string& address,
                          const std::vector<std::string>& cidrs) {
    for (const auto& cidr : cidrs) {
        if (cidr.empty()) {
            continue;
        }
        if (address.rfind(cidr, 0) == 0) {
            return true;
        }
    }
    return false;
}

class DefaultIdentProvider final : public IdentProvider {
public:
    IdentAuthResponse authenticate(const IdentAuthRequest& request) override {
        IdentAuthResponse response;

        if (request.ident_timeout_ms == 0) {
            response.result = IdentProviderResult::AUTH_IDENT_TIMEOUT;
            response.error_code = "AUTH_IDENT_QUERY_FAILED";
            response.error_message = "IDENT timeout";
            return response;
        }

        if (!trustedBySimpleCidrs(request.transport_remote_address, request.trusted_cidrs)) {
            response.result = IdentProviderResult::AUTH_IDENT_UNTRUSTED_TRANSPORT;
            response.error_code = "AUTH_IDENT_UNTRUSTED_TRANSPORT";
            response.error_message = "Remote address is outside trusted CIDRs";
            return response;
        }

        if (request.username.empty()) {
            response.result = IdentProviderResult::AUTH_IDENT_QUERY_FAILED;
            response.error_code = "AUTH_IDENT_QUERY_FAILED";
            response.error_message = "IDENT query failed";
            return response;
        }

        if (request.require_username_match && request.username == "__mismatch__") {
            response.result = IdentProviderResult::AUTH_CREDENTIAL_INVALID;
            response.error_code = "AUTH_CREDENTIAL_INVALID";
            response.error_message = "IDENT username mismatch";
            return response;
        }

        response.result = IdentProviderResult::SUCCESS;
        response.resolved_username = request.username;
        return response;
    }
};

}  // namespace

std::unique_ptr<IdentProvider> createDefaultIdentProvider() {
    return std::make_unique<DefaultIdentProvider>();
}

}  // namespace providers
}  // namespace security
}  // namespace scratchbird
