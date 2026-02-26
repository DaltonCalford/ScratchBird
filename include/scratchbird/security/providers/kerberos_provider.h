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

enum class KerberosProviderResult : uint8_t {
    SUCCESS = 0,
    AUTH_KERBEROS_TICKET_INVALID,
    AUTH_KERBEROS_REPLAY_DETECTED,
    AUTH_KERBEROS_SPN_MISMATCH,
    AUTH_PLUGIN_POLICY_DENIED,
    AUTH_KERBEROS_TIMEOUT,
    INTERNAL_ERROR,
};

struct KerberosAuthRequest {
    std::string username;
    std::string ticket_b64;
    std::string service_principal;
    std::string keytab_path;
    bool allow_delegation = false;
    uint32_t max_replay_window_ms = 30000;
    uint32_t connect_timeout_ms = 3000;
    std::vector<std::string> allowed_kdc_endpoints;
    std::string kdc_endpoint;
};

struct KerberosAuthResponse {
    KerberosProviderResult result = KerberosProviderResult::INTERNAL_ERROR;
    std::string error_code;
    std::string error_message;
    std::string resolved_principal;
};

class KerberosProvider {
public:
    virtual ~KerberosProvider() = default;
    virtual KerberosAuthResponse authenticate(const KerberosAuthRequest& request) = 0;
};

std::unique_ptr<KerberosProvider> createDefaultKerberosProvider();

}  // namespace providers
}  // namespace security
}  // namespace scratchbird
