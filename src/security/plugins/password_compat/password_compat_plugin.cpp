/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "../auth_plugin_observability.h"
#include "../auth_plugin_secret_provider.h"

#include "scratchbird/security/auth_plugin_abi_v1.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.password_compat";
constexpr char kPluginVersion[] = "1.0.0";
constexpr char kMethodPasswordCompat[] = "scratchbird.auth.password_compat";
constexpr char kMethodMd5Legacy[] = "scratchbird.auth.md5_legacy";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxSecretBytes = 1024;

constexpr char kPolicyKeyMode[] = "auth.password_compat.mode";
constexpr char kModeBootstrapAllow[] = "bootstrap_allow";
constexpr char kPolicyAllowMd5Legacy[] = "auth.password_compat.allow_md5_legacy";
constexpr char kPolicyCredentialRefPrefix[] = "auth.password_compat.credential_ref.";
constexpr char kPolicyDefaultCredentialRef[] = "auth.password_compat.default_credential_ref";

constexpr char kErrBadRequest[] = "AUTH_PASSWORD_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_PASSWORD_METHOD_UNKNOWN";
constexpr char kErrMissingSecret[] = "AUTH_PASSWORD_MISSING_SECRET";
constexpr char kErrOversizedSecret[] = "AUTH_PASSWORD_OVERSIZED_SECRET";
constexpr char kErrMd5Denied[] = "AUTH_PASSWORD_MD5_LEGACY_DENIED";
constexpr char kErrCompatDisabled[] = "AUTH_PASSWORD_COMPAT_DISABLED";
constexpr char kErrMissingUsername[] = "AUTH_PASSWORD_USERNAME_MISSING";
constexpr char kErrCredentialRefMissing[] = "AUTH_PASSWORD_CREDENTIAL_REF_MISSING";
constexpr char kErrCredentialResolveFailed[] = "AUTH_PASSWORD_CREDENTIAL_RESOLVE_FAILED";
constexpr char kErrCredentialMismatch[] = "AUTH_PASSWORD_CREDENTIAL_MISMATCH";
constexpr char kErrNoContinuation[] = "AUTH_PASSWORD_NO_CONTINUE";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};
scratchbird::security::plugins::observability::OutcomeCounters g_counters{};
thread_local std::string g_health_payload;

inline void copyCStr(char* out, std::size_t out_size, const char* value) {
    if (!out || out_size == 0) {
        return;
    }
    if (!value) {
        out[0] = '\0';
        return;
    }
    std::strncpy(out, value, out_size - 1);
    out[out_size - 1] = '\0';
}

inline bool sliceEqualsLiteral(sb_auth_slice_t value, const char* literal) {
    if (!literal || !value.ptr) {
        return false;
    }
    const std::size_t literal_len = std::strlen(literal);
    if (value.len != literal_len) {
        return false;
    }
    return std::memcmp(value.ptr, literal, literal_len) == 0;
}

inline std::string toString(sb_auth_slice_t value) {
    if (!value.ptr || value.len == 0) {
        return "";
    }
    const char* begin = reinterpret_cast<const char*>(value.ptr);
    return std::string(begin, begin + value.len);
}

inline bool parsePolicyBoolean(const std::string& value, bool* out_bool) {
    if (!out_bool) {
        return false;
    }

    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            normalized.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            normalized.push_back(ch);
        }
    }

    if (normalized == "1" || normalized == "true" || normalized == "yes" ||
        normalized == "on") {
        *out_bool = true;
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" ||
        normalized == "off") {
        *out_bool = false;
        return true;
    }
    return false;
}

inline bool readPolicyValueString(const std::string& key, std::string* out_value) {
    if (!out_value || !g_host_api || !g_host_api->read_policy_value || key.empty()) {
        return false;
    }

    const sb_auth_slice_t key_slice{
        reinterpret_cast<const uint8_t*>(key.data()),
        static_cast<uint32_t>(key.size())
    };
    sb_auth_slice_t value_slice{};
    if (g_host_api->read_policy_value(key_slice, &value_slice) != SB_AUTH_RC_OK || !value_slice.ptr ||
        value_slice.len == 0) {
        return false;
    }

    *out_value = toString(value_slice);
    return !out_value->empty();
}

inline void zeroAndPrimeResult(sb_auth_step_result_v1* out_result) {
    if (!out_result) {
        return;
    }
    std::memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = sizeof(sb_auth_step_result_v1);
    out_result->principal.struct_size = sizeof(sb_auth_principal_v1);
}

inline void fillDeniedResult(sb_auth_step_result_v1* out_result,
                             uint32_t plugin_error_numeric,
                             const char* plugin_error_code) {
    if (!out_result) {
        return;
    }
    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_DENY;
    out_result->plugin_error_numeric = plugin_error_numeric;
    copyCStr(out_result->plugin_error_code,
             sizeof(out_result->plugin_error_code),
             plugin_error_code);
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "28000");
}

inline bool compatModeAllowsBootstrap() {
    std::string mode;
    if (!readPolicyValueString(kPolicyKeyMode, &mode)) {
        return false;
    }
    return mode == kModeBootstrapAllow;
}

inline bool md5LegacyAllowed() {
    std::string policy_value;
    if (!readPolicyValueString(kPolicyAllowMd5Legacy, &policy_value)) {
        return false;
    }
    bool enabled = false;
    if (!parsePolicyBoolean(policy_value, &enabled)) {
        return false;
    }
    return enabled;
}

inline bool resolveCredentialSecretRef(sb_auth_slice_t username, std::string* out_ref) {
    if (!out_ref || !username.ptr || username.len == 0) {
        return false;
    }

    const std::string user_text = toString(username);
    const std::string user_key =
        std::string(kPolicyCredentialRefPrefix) + user_text;
    if (readPolicyValueString(user_key, out_ref)) {
        return true;
    }
    return readPolicyValueString(kPolicyDefaultCredentialRef, out_ref);
}

inline bool secretsEqual(sb_auth_slice_t lhs, const std::string& rhs) {
    if (!lhs.ptr || lhs.len != rhs.size()) {
        return false;
    }

    const auto* rhs_ptr = reinterpret_cast<const uint8_t*>(rhs.data());
    if (g_host_api && g_host_api->const_time_equal) {
        return g_host_api->const_time_equal(lhs.ptr,
                                            rhs_ptr,
                                            static_cast<uint32_t>(rhs.size())) != 0;
    }
    return std::memcmp(lhs.ptr, rhs_ptr, rhs.size()) == 0;
}

inline sb_auth_rc_t createInstance(sb_auth_plugin_instance_t* out_instance) {
    if (!out_instance) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    *out_instance = g_next_instance.fetch_add(1, std::memory_order_relaxed);
    return SB_AUTH_RC_OK;
}

inline void destroyInstance(sb_auth_plugin_instance_t /*instance*/) {}

inline sb_auth_rc_t configureInstance(sb_auth_plugin_instance_t /*instance*/,
                                      sb_auth_slice_t config_json) {
    if (config_json.len == 0) {
        return SB_AUTH_RC_OK;
    }
    if (!config_json.ptr) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    return SB_AUTH_RC_UNSUPPORTED;
}

inline sb_auth_rc_t beginAuth(sb_auth_plugin_instance_t /*instance*/,
                              sb_auth_slice_t method_id,
                              const sb_auth_connection_ctx_v1* conn,
                              sb_auth_slice_t client_payload,
                              sb_auth_exchange_t* inout_exchange,
                              sb_auth_step_result_v1* out_result) {
    auto finish = [&](sb_auth_rc_t rc, const char* method, const char* error_code) {
        scratchbird::security::plugins::observability::recordOutcome(g_counters, rc);
        scratchbird::security::plugins::observability::emitAuditEvent(
            g_host_api,
            "auth_plugin.password_compat.begin",
            kPluginId,
            method,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result) {
        fillDeniedResult(out_result, 4001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, nullptr, kErrBadRequest);
    }
    *inout_exchange = 0;

    const bool is_md5_legacy = sliceEqualsLiteral(method_id, kMethodMd5Legacy);
    if (sliceEqualsLiteral(method_id, kMethodMd5Legacy)) {
        if (!md5LegacyAllowed()) {
            fillDeniedResult(out_result, 4005, kErrMd5Denied);
            return finish(SB_AUTH_RC_POLICY_VIOLATION, kMethodMd5Legacy, kErrMd5Denied);
        }
    }

    if (!is_md5_legacy && !sliceEqualsLiteral(method_id, kMethodPasswordCompat)) {
        fillDeniedResult(out_result, 4002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, nullptr, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 4003, kErrMissingSecret);
        return finish(SB_AUTH_RC_DENY, kMethodPasswordCompat, kErrMissingSecret);
    }
    if (client_payload.len > kMaxSecretBytes) {
        fillDeniedResult(out_result, 4004, kErrOversizedSecret);
        return finish(SB_AUTH_RC_DENY, kMethodPasswordCompat, kErrOversizedSecret);
    }

    if (!is_md5_legacy && !compatModeAllowsBootstrap()) {
        fillDeniedResult(out_result, 4006, kErrCompatDisabled);
        return finish(SB_AUTH_RC_DENY, kMethodPasswordCompat, kErrCompatDisabled);
    }

    if (!conn || !conn->username.ptr || conn->username.len == 0) {
        fillDeniedResult(out_result, 4008, kErrMissingUsername);
        return finish(SB_AUTH_RC_DENY,
                      is_md5_legacy ? kMethodMd5Legacy : kMethodPasswordCompat,
                      kErrMissingUsername);
    }

    std::string credential_ref;
    if (!resolveCredentialSecretRef(conn->username, &credential_ref)) {
        fillDeniedResult(out_result, 4009, kErrCredentialRefMissing);
        return finish(SB_AUTH_RC_DENY,
                      is_md5_legacy ? kMethodMd5Legacy : kMethodPasswordCompat,
                      kErrCredentialRefMissing);
    }

    scratchbird::security::plugins::secrets::SecretMaterial credential_secret{};
    if (scratchbird::security::plugins::secrets::resolveSecretReference(
            g_host_api,
            credential_ref,
            &credential_secret) != scratchbird::security::plugins::secrets::SecretResolveStatus::OK) {
        fillDeniedResult(out_result, 4010, kErrCredentialResolveFailed);
        return finish(SB_AUTH_RC_DENY,
                      is_md5_legacy ? kMethodMd5Legacy : kMethodPasswordCompat,
                      kErrCredentialResolveFailed);
    }

    if (!secretsEqual(client_payload, credential_secret.value)) {
        fillDeniedResult(out_result, 4011, kErrCredentialMismatch);
        return finish(SB_AUTH_RC_DENY,
                      is_md5_legacy ? kMethodMd5Legacy : kMethodPasswordCompat,
                      kErrCredentialMismatch);
    }

    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = is_md5_legacy ? 3 : 5;
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");

    out_result->principal.resolved_username = conn->username;
    if (g_host_api && g_host_api->resolve_user_by_name) {
        (void) g_host_api->resolve_user_by_name(conn->username,
                                                out_result->principal.principal_uuid);
    }

    return finish(SB_AUTH_RC_OK,
                  is_md5_legacy ? kMethodMd5Legacy : kMethodPasswordCompat,
                  nullptr);
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t /*exchange*/,
                                 sb_auth_slice_t /*client_payload*/,
                                 sb_auth_step_result_v1* out_result) {
    fillDeniedResult(out_result, 4007, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.password_compat.continue",
        kPluginId,
        kMethodPasswordCompat,
        SB_AUTH_RC_DENY,
        kErrNoContinuation);
    return SB_AUTH_RC_DENY;
}

inline void abortAuth(sb_auth_plugin_instance_t /*instance*/,
                      sb_auth_exchange_t /*exchange*/) {}

inline sb_auth_rc_t healthCheck(sb_auth_plugin_instance_t /*instance*/,
                                sb_auth_slice_t* out_json) {
    if (!out_json) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    g_health_payload = "{\"status\":\"ok\",\"plugin\":\"password_compat\",\"allow_count\":" +
                       std::to_string(
                           scratchbird::security::plugins::observability::loadCounter(g_counters.allow)) +
                       ",\"deny_count\":" +
                       std::to_string(
                           scratchbird::security::plugins::observability::loadCounter(g_counters.deny)) +
                       ",\"continue_count\":" +
                       std::to_string(
                           scratchbird::security::plugins::observability::loadCounter(g_counters.cont)) +
                       ",\"error_count\":" +
                       std::to_string(
                           scratchbird::security::plugins::observability::loadCounter(g_counters.error)) +
                       "}";
    out_json->ptr = reinterpret_cast<const uint8_t*>(g_health_payload.data());
    out_json->len = static_cast<uint32_t>(g_health_payload.size());
    return SB_AUTH_RC_OK;
}

inline const sb_auth_plugin_descriptor_v1* descriptor() {
    static const std::array<sb_auth_method_descriptor_v1, 2> kMethods = [] {
        std::array<sb_auth_method_descriptor_v1, 2> methods{};

        std::memset(&methods[0], 0, sizeof(methods[0]));
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodPasswordCompat);
        methods[0].method_flags = 0;
        methods[0].legacy_wire_code = kLegacyWireUnknown;

        std::memset(&methods[1], 0, sizeof(methods[1]));
        copyCStr(methods[1].method_id, sizeof(methods[1].method_id), kMethodMd5Legacy);
        methods[1].method_flags = 0;
        methods[1].legacy_wire_code = kLegacyWireUnknown;

        return methods;
    }();

    static const sb_auth_slice_t kVersion{
        reinterpret_cast<const uint8_t*>(kPluginVersion),
        static_cast<uint32_t>(sizeof(kPluginVersion) - 1)
    };

    static const sb_auth_plugin_descriptor_v1 kDescriptor = [] {
        sb_auth_plugin_descriptor_v1 desc{};
        std::memset(&desc, 0, sizeof(desc));
        desc.struct_size = sizeof(sb_auth_plugin_descriptor_v1);
        copyCStr(desc.plugin_id, sizeof(desc.plugin_id), kPluginId);
        desc.plugin_version = kVersion;
        desc.abi_major = SB_AUTH_ABI_MAJOR;
        desc.abi_minor = SB_AUTH_ABI_MINOR;
        desc.method_count = static_cast<uint32_t>(kMethods.size());
        desc.methods = kMethods.data();
        return desc;
    }();

    return &kDescriptor;
}

inline const sb_auth_plugin_api_v1* api() {
    static const sb_auth_plugin_api_v1 kApi = {
        sizeof(sb_auth_plugin_api_v1),
        &createInstance,
        &destroyInstance,
        &configureInstance,
        &beginAuth,
        &continueAuth,
        &abortAuth,
        &healthCheck,
    };
    return &kApi;
}

}  // namespace

extern "C" sb_auth_rc_t sb_auth_plugin_get_api_v1(
    uint32_t requested_abi_major,
    const sb_auth_host_api_v1* host_api,
    const sb_auth_plugin_descriptor_v1** out_descriptor,
    const sb_auth_plugin_api_v1** out_api) {
    if (!host_api || !out_descriptor || !out_api) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    if (requested_abi_major != SB_AUTH_ABI_MAJOR) {
        return SB_AUTH_RC_UNSUPPORTED;
    }

    g_host_api = host_api;
    *out_descriptor = descriptor();
    *out_api = api();
    return SB_AUTH_RC_OK;
}
