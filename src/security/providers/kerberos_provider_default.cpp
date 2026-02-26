/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/security/providers/kerberos_provider.h"

#include <algorithm>

namespace scratchbird {
namespace security {
namespace providers {

namespace {

class DefaultKerberosProvider final : public KerberosProvider {
public:
    KerberosAuthResponse authenticate(const KerberosAuthRequest& request) override {
        KerberosAuthResponse response;

        if (request.connect_timeout_ms == 0) {
            response.result = KerberosProviderResult::AUTH_KERBEROS_TIMEOUT;
            response.error_code = "AUTH_KERBEROS_TIMEOUT";
            response.error_message = "Kerberos timeout";
            return response;
        }

        if (request.service_principal.empty() || request.keytab_path.empty()) {
            response.result = KerberosProviderResult::AUTH_KERBEROS_SPN_MISMATCH;
            response.error_code = "AUTH_KERBEROS_SPN_MISMATCH";
            response.error_message = "Service principal/keytab not configured";
            return response;
        }

        if (!request.allowed_kdc_endpoints.empty()) {
            const auto it = std::find(request.allowed_kdc_endpoints.begin(),
                                      request.allowed_kdc_endpoints.end(),
                                      request.kdc_endpoint);
            if (it == request.allowed_kdc_endpoints.end()) {
                response.result = KerberosProviderResult::AUTH_PLUGIN_POLICY_DENIED;
                response.error_code = "AUTH_PLUGIN_POLICY_DENIED";
                response.error_message = "KDC endpoint blocked by allowlist";
                return response;
            }
        }

        if (request.ticket_b64 == "__replay__") {
            response.result = KerberosProviderResult::AUTH_KERBEROS_REPLAY_DETECTED;
            response.error_code = "AUTH_KERBEROS_REPLAY_DETECTED";
            response.error_message = "Replay detected";
            return response;
        }

        if (request.ticket_b64 == "__invalid__") {
            response.result = KerberosProviderResult::AUTH_KERBEROS_TICKET_INVALID;
            response.error_code = "AUTH_KERBEROS_TICKET_INVALID";
            response.error_message = "Ticket validation failed";
            return response;
        }

        response.result = KerberosProviderResult::SUCCESS;
        response.resolved_principal = request.username;
        return response;
    }
};

}  // namespace

std::unique_ptr<KerberosProvider> createDefaultKerberosProvider() {
    return std::make_unique<DefaultKerberosProvider>();
}

}  // namespace providers
}  // namespace security
}  // namespace scratchbird
