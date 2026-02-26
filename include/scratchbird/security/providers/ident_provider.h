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

enum class IdentProviderResult : uint8_t {
    SUCCESS = 0,
    AUTH_IDENT_QUERY_FAILED,
    AUTH_IDENT_UNTRUSTED_TRANSPORT,
    AUTH_CREDENTIAL_INVALID,
    AUTH_IDENT_TIMEOUT,
    INTERNAL_ERROR,
};

struct IdentAuthRequest {
    std::string username;
    std::string transport_remote_address;
    uint32_t ident_timeout_ms = 1000;
    std::vector<std::string> trusted_cidrs;
    bool require_username_match = true;
};

struct IdentAuthResponse {
    IdentProviderResult result = IdentProviderResult::INTERNAL_ERROR;
    std::string error_code;
    std::string error_message;
    std::string resolved_username;
};

class IdentProvider {
public:
    virtual ~IdentProvider() = default;
    virtual IdentAuthResponse authenticate(const IdentAuthRequest& request) = 0;
};

std::unique_ptr<IdentProvider> createDefaultIdentProvider();

}  // namespace providers
}  // namespace security
}  // namespace scratchbird
