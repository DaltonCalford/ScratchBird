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
#include "../auth_plugin_crypto.h"
#include "../auth_plugin_secret_provider.h"

#include "scratchbird/security/auth_plugin_abi_v1.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <string>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.token_authkey";
constexpr char kPluginVersion[] = "1.0.0";
constexpr char kMethodAuthKeyToken[] = "scratchbird.auth.authkey_token";
constexpr char kTokenIssuer[] = "scratchbird.auth.authkey";
constexpr char kSigningKeyPolicyRef[] = "policy:auth.token_authkey.signing_key_ref";
constexpr char kPolicyExpectedIssuer[] = "auth.token_authkey.expected_issuer";
constexpr char kPolicyExpectedAudience[] = "auth.token_authkey.expected_audience";
constexpr char kDefaultAudience[] = "scratchbird";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxTokenBytes = 4096;
constexpr uint32_t kMaxIssuerBytes = 128;
constexpr uint32_t kMaxAudienceBytes = 128;
constexpr uint32_t kMaxSubjectBytes = 256;
constexpr uint32_t kExpectedSignatureHexBytes = 64;

constexpr char kErrBadRequest[] = "AUTH_TOKEN_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_TOKEN_METHOD_UNKNOWN";
constexpr char kErrMissingToken[] = "AUTH_TOKEN_MISSING";
constexpr char kErrOversizedToken[] = "AUTH_TOKEN_OVERSIZED";
constexpr char kErrMalformedToken[] = "AUTH_TOKEN_MALFORMED";
constexpr char kErrSigningKeyUnavailable[] = "AUTH_TOKEN_SIGNING_KEY_UNAVAILABLE";
constexpr char kErrIssuerMismatch[] = "AUTH_TOKEN_ISSUER_MISMATCH";
constexpr char kErrAudienceMismatch[] = "AUTH_TOKEN_AUDIENCE_MISMATCH";
constexpr char kErrExpired[] = "AUTH_TOKEN_EXPIRED";
constexpr char kErrSignatureInvalid[] = "AUTH_TOKEN_SIGNATURE_INVALID";
constexpr char kErrResolverUnavailable[] = "AUTH_TOKEN_RESOLVER_UNAVAILABLE";
constexpr char kErrSubjectUnknown[] = "AUTH_TOKEN_SUBJECT_UNKNOWN";
constexpr char kErrNoContinuation[] = "AUTH_TOKEN_NO_CONTINUE";

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

inline bool tokenLooksSafe(sb_auth_slice_t token) {
    if (!token.ptr || token.len == 0 || token.len > kMaxTokenBytes) {
        return false;
    }

    for (uint32_t i = 0; i < token.len; ++i) {
        const uint8_t c = token.ptr[i];
        const bool allowed =
            (c >= static_cast<uint8_t>('a') && c <= static_cast<uint8_t>('z')) ||
            (c >= static_cast<uint8_t>('A') && c <= static_cast<uint8_t>('Z')) ||
            (c >= static_cast<uint8_t>('0') && c <= static_cast<uint8_t>('9')) ||
            c == static_cast<uint8_t>('-') || c == static_cast<uint8_t>('_') ||
            c == static_cast<uint8_t>('.') || c == static_cast<uint8_t>(':') ||
            c == static_cast<uint8_t>('=') || c == static_cast<uint8_t>(';');
        if (!allowed) {
            return false;
        }
    }

    return true;
}

inline std::string toString(sb_auth_slice_t value) {
    if (!value.ptr || value.len == 0) {
        return "";
    }
    const char* begin = reinterpret_cast<const char*>(value.ptr);
    return std::string(begin, begin + value.len);
}

struct ParsedToken {
    std::string issuer;
    std::string audience;
    std::string subject;
    uint64_t exp_unix_ms = 0;
    std::string signature;
};

inline uint64_t nowUnixMs() {
    if (g_host_api && g_host_api->now_unix_ms) {
        return g_host_api->now_unix_ms();
    }
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

inline bool parseUInt64(const std::string& text, uint64_t* out_value) {
    if (!out_value || text.empty()) {
        return false;
    }

    uint64_t acc = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        const uint64_t digit = static_cast<uint64_t>(ch - '0');
        const uint64_t next = acc * 10u + digit;
        if (next < acc) {
            return false;
        }
        acc = next;
    }
    *out_value = acc;
    return true;
}

inline bool isBasicTokenFieldChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '-' || ch == '_' || ch == '.' || ch == ':' || ch == '/';
}

inline bool isHexLower(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

inline bool parseBoundedTokenField(std::string_view value, uint32_t max_bytes) {
    if (value.empty() || value.size() > max_bytes) {
        return false;
    }
    for (char ch : value) {
        if (!isBasicTokenFieldChar(ch)) {
            return false;
        }
    }
    return true;
}

inline bool parseToken(std::string_view token, ParsedToken* out_token) {
    if (!out_token || token.empty()) {
        return false;
    }

    ParsedToken parsed{};
    bool issuer_seen = false;
    bool audience_seen = false;
    bool subject_seen = false;
    bool exp_seen = false;
    bool signature_seen = false;
    std::size_t cursor = 0;
    while (cursor < token.size()) {
        const std::size_t sep = token.find(';', cursor);
        const std::size_t end = (sep == std::string_view::npos) ? token.size() : sep;
        if (end <= cursor) {
            return false;
        }
        const std::string_view pair = token.substr(cursor, end - cursor);
        const std::size_t eq = pair.find('=');
        if (eq == std::string_view::npos || eq == 0 || eq + 1 >= pair.size()) {
            return false;
        }

        const std::string_view key = pair.substr(0, eq);
        const std::string_view value = pair.substr(eq + 1);
        if (key == "iss") {
            if (issuer_seen || !parseBoundedTokenField(value, kMaxIssuerBytes)) {
                return false;
            }
            parsed.issuer.assign(value.begin(), value.end());
            issuer_seen = true;
        } else if (key == "aud") {
            if (audience_seen || !parseBoundedTokenField(value, kMaxAudienceBytes)) {
                return false;
            }
            parsed.audience.assign(value.begin(), value.end());
            audience_seen = true;
        } else if (key == "sub") {
            if (subject_seen || !parseBoundedTokenField(value, kMaxSubjectBytes)) {
                return false;
            }
            parsed.subject.assign(value.begin(), value.end());
            subject_seen = true;
        } else if (key == "exp") {
            if (exp_seen) {
                return false;
            }
            std::string exp_text(value.begin(), value.end());
            if (!parseUInt64(exp_text, &parsed.exp_unix_ms)) {
                return false;
            }
            exp_seen = true;
        } else if (key == "sig") {
            if (signature_seen || value.size() != kExpectedSignatureHexBytes) {
                return false;
            }
            for (char ch : value) {
                if (!isHexLower(ch)) {
                    return false;
                }
            }
            parsed.signature.assign(value.begin(), value.end());
            signature_seen = true;
        } else {
            return false;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (!issuer_seen || !audience_seen || !subject_seen || !exp_seen || !signature_seen ||
        parsed.exp_unix_ms == 0) {
        return false;
    }

    *out_token = std::move(parsed);
    return true;
}

inline std::string computeSignature(const ParsedToken& token, const std::string& signing_key) {
    const std::string message = token.issuer + "|" + token.audience + "|" + token.subject + "|" +
                                std::to_string(token.exp_unix_ms);
    std::string signature;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(signing_key, message, &signature)) {
        return "";
    }
    return signature;
}

inline bool loadPolicyString(const char* key, std::string* out) {
    if (!key || !out || !g_host_api || !g_host_api->read_policy_value) {
        return false;
    }
    const sb_auth_slice_t key_slice{
        reinterpret_cast<const uint8_t*>(key),
        static_cast<uint32_t>(std::strlen(key))
    };
    sb_auth_slice_t value{};
    if (g_host_api->read_policy_value(key_slice, &value) != SB_AUTH_RC_OK || !value.ptr ||
        value.len == 0) {
        return false;
    }
    const char* begin = reinterpret_cast<const char*>(value.ptr);
    *out = std::string(begin, begin + value.len);
    return !out->empty();
}

inline bool signaturesEqual(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    if (g_host_api && g_host_api->const_time_equal) {
        return g_host_api->const_time_equal(
                   reinterpret_cast<const uint8_t*>(lhs.data()),
                   reinterpret_cast<const uint8_t*>(rhs.data()),
                   static_cast<uint32_t>(lhs.size())) != 0;
    }
    return std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
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
    auto finish = [&](sb_auth_rc_t rc, const char* error_code) {
        scratchbird::security::plugins::observability::recordOutcome(g_counters, rc);
        scratchbird::security::plugins::observability::emitAuditEvent(
            g_host_api,
            "auth_plugin.token_authkey.begin",
            kPluginId,
            kMethodAuthKeyToken,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result) {
        fillDeniedResult(out_result, 3001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }
    *inout_exchange = 0;

    if (!sliceEqualsLiteral(method_id, kMethodAuthKeyToken)) {
        fillDeniedResult(out_result, 3002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 3003, kErrMissingToken);
        return finish(SB_AUTH_RC_DENY, kErrMissingToken);
    }
    if (client_payload.len > kMaxTokenBytes) {
        fillDeniedResult(out_result, 3004, kErrOversizedToken);
        return finish(SB_AUTH_RC_DENY, kErrOversizedToken);
    }
    if (!tokenLooksSafe(client_payload)) {
        fillDeniedResult(out_result, 3005, kErrMalformedToken);
        return finish(SB_AUTH_RC_DENY, kErrMalformedToken);
    }

    ParsedToken token{};
    if (!parseToken(toString(client_payload), &token)) {
        fillDeniedResult(out_result, 3005, kErrMalformedToken);
        return finish(SB_AUTH_RC_DENY, kErrMalformedToken);
    }

    scratchbird::security::plugins::secrets::SecretMaterial signing_key{};
    if (scratchbird::security::plugins::secrets::resolveSecretReference(
            g_host_api,
            kSigningKeyPolicyRef,
            &signing_key) != scratchbird::security::plugins::secrets::SecretResolveStatus::OK) {
        fillDeniedResult(out_result, 3009, kErrSigningKeyUnavailable);
        return finish(SB_AUTH_RC_DENY, kErrSigningKeyUnavailable);
    }
    (void) signing_key;

    std::string expected_issuer = kTokenIssuer;
    std::string expected_audience = kDefaultAudience;
    (void) loadPolicyString(kPolicyExpectedIssuer, &expected_issuer);
    (void) loadPolicyString(kPolicyExpectedAudience, &expected_audience);

    if (token.issuer != expected_issuer) {
        fillDeniedResult(out_result, 3010, kErrIssuerMismatch);
        return finish(SB_AUTH_RC_DENY, kErrIssuerMismatch);
    }
    if (token.audience != expected_audience) {
        fillDeniedResult(out_result, 3011, kErrAudienceMismatch);
        return finish(SB_AUTH_RC_DENY, kErrAudienceMismatch);
    }
    if (token.exp_unix_ms <= nowUnixMs()) {
        fillDeniedResult(out_result, 3012, kErrExpired);
        return finish(SB_AUTH_RC_DENY, kErrExpired);
    }

    const std::string expected_signature = computeSignature(token, signing_key.value);
    if (!signaturesEqual(expected_signature, token.signature)) {
        fillDeniedResult(out_result, 3013, kErrSignatureInvalid);
        return finish(SB_AUTH_RC_SIGNATURE_INVALID, kErrSignatureInvalid);
    }

    if (!g_host_api || !g_host_api->resolve_user_by_external_subject) {
        fillDeniedResult(out_result, 3006, kErrResolverUnavailable);
        return finish(SB_AUTH_RC_DENY, kErrResolverUnavailable);
    }

    const sb_auth_slice_t issuer{
        reinterpret_cast<const uint8_t*>(kTokenIssuer),
        static_cast<uint32_t>(sizeof(kTokenIssuer) - 1)
    };

    zeroAndPrimeResult(out_result);
    const sb_auth_rc_t resolve_rc = g_host_api->resolve_user_by_external_subject(
        issuer,
        sb_auth_slice_t{
            reinterpret_cast<const uint8_t*>(token.subject.data()),
            static_cast<uint32_t>(token.subject.size())
        },
        out_result->principal.principal_uuid);
    if (resolve_rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result, 3007, kErrSubjectUnknown);
        return finish(SB_AUTH_RC_DENY, kErrSubjectUnknown);
    }

    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 60;
    out_result->principal.external_subject = client_payload;
    if (conn && conn->username.ptr && conn->username.len > 0) {
        out_result->principal.resolved_username = conn->username;
    }
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
    return finish(SB_AUTH_RC_OK, nullptr);
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t /*exchange*/,
                                 sb_auth_slice_t /*client_payload*/,
                                 sb_auth_step_result_v1* out_result) {
    fillDeniedResult(out_result, 3008, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.token_authkey.continue",
        kPluginId,
        kMethodAuthKeyToken,
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
    g_health_payload = "{\"status\":\"ok\",\"plugin\":\"token_authkey\",\"allow_count\":" +
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
    static const std::array<sb_auth_method_descriptor_v1, 1> kMethods = [] {
        std::array<sb_auth_method_descriptor_v1, 1> methods{};
        std::memset(&methods[0], 0, sizeof(methods[0]));
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodAuthKeyToken);
        methods[0].method_flags = 0;
        methods[0].legacy_wire_code = kLegacyWireUnknown;
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
