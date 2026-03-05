/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "scratchbird/security/auth_plugin_abi_v1.h"

namespace scratchbird {
namespace security {
namespace plugins {
namespace secrets {

enum class SecretResolveStatus : uint8_t {
    OK = 0,
    MISSING_REFERENCE,
    MISSING_HOST_API,
    MISSING_POLICY_READER,
    POLICY_READ_FAILED,
    EMPTY_SECRET,
};

struct SecretMaterial {
    std::string value;
    std::string source;  // "policy" or "literal"
    bool supports_rotation = false;
};

inline bool startsWith(const std::string& value, const char* prefix) {
    if (!prefix) {
        return false;
    }
    const std::size_t n = std::char_traits<char>::length(prefix);
    return value.size() >= n && value.compare(0, n, prefix) == 0;
}

inline std::string toStringCopy(sb_auth_slice_t value) {
    if (!value.ptr || value.len == 0) {
        return "";
    }
    const char* begin = reinterpret_cast<const char*>(value.ptr);
    return std::string(begin, begin + value.len);
}

inline SecretResolveStatus resolveSecretReference(const sb_auth_host_api_v1* host_api,
                                                  const std::string& secret_ref,
                                                  SecretMaterial* out_secret,
                                                  std::string* error_out = nullptr) {
    auto set_error = [error_out](const std::string& value) {
        if (error_out) {
            *error_out = value;
        }
    };

    if (secret_ref.empty()) {
        set_error("Secret reference is empty");
        return SecretResolveStatus::MISSING_REFERENCE;
    }

    if (startsWith(secret_ref, "literal:")) {
        SecretMaterial material{};
        material.value = secret_ref.substr(8);
        material.source = "literal";
        material.supports_rotation = false;
        if (material.value.empty()) {
            set_error("Literal secret resolved to empty value");
            return SecretResolveStatus::EMPTY_SECRET;
        }
        if (out_secret) {
            *out_secret = std::move(material);
        }
        return SecretResolveStatus::OK;
    }

    if (!host_api) {
        set_error("Host API unavailable for policy secret resolution");
        return SecretResolveStatus::MISSING_HOST_API;
    }
    if (!host_api->read_policy_value) {
        set_error("Host policy reader unavailable for secret resolution");
        return SecretResolveStatus::MISSING_POLICY_READER;
    }

    std::string policy_key = secret_ref;
    if (startsWith(secret_ref, "policy:")) {
        policy_key = secret_ref.substr(7);
    }
    if (policy_key.empty()) {
        set_error("Policy secret reference key is empty");
        return SecretResolveStatus::MISSING_REFERENCE;
    }

    const sb_auth_slice_t key_slice{
        reinterpret_cast<const uint8_t*>(policy_key.data()),
        static_cast<uint32_t>(policy_key.size())
    };
    sb_auth_slice_t value_slice{};
    if (host_api->read_policy_value(key_slice, &value_slice) != SB_AUTH_RC_OK) {
        set_error("Failed to resolve policy secret reference");
        return SecretResolveStatus::POLICY_READ_FAILED;
    }

    SecretMaterial material{};
    material.value = toStringCopy(value_slice);
    material.source = "policy";
    material.supports_rotation = true;
    if (material.value.empty()) {
        set_error("Policy secret resolved to empty value");
        return SecretResolveStatus::EMPTY_SECRET;
    }

    if (out_secret) {
        *out_secret = std::move(material);
    }
    return SecretResolveStatus::OK;
}

}  // namespace secrets
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
