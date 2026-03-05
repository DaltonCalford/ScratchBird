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
#include <string>
#include <string_view>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.proxy_assertion";
constexpr char kPluginVersion[] = "1.0.0";
constexpr char kMethodProxyPrincipalAssertion[] = "scratchbird.auth.proxy_principal_assertion";
constexpr char kDefaultIssuer[] = "proxy";
constexpr char kSigningKeyPolicyRef[] = "policy:auth.proxy_assertion.signing_key_ref";
constexpr char kPolicyAllowInet[] = "auth.proxy_assertion.allow_inet";
constexpr char kPolicyTrustedProxyUid[] = "auth.proxy_assertion.trusted_proxy_uid";
constexpr char kPolicyExpectedIssuer[] = "auth.proxy_assertion.expected_issuer";
constexpr char kPolicyRequiredAudience[] = "auth.proxy_assertion.required_audience";
constexpr char kPolicyExpectedProxyId[] = "auth.proxy_assertion.expected_proxy_id";

constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxPayloadBytes = 2048;
constexpr uint32_t kMaxIssuerBytes = 128;
constexpr uint32_t kMaxSubjectBytes = 256;
constexpr uint32_t kMaxAudienceBytes = 128;
constexpr uint32_t kMaxProxyIdBytes = 128;
constexpr uint32_t kExpectedSignatureHexBytes = 64;

constexpr char kErrBadRequest[] = "AUTH_PROXY_ASSERTION_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_PROXY_ASSERTION_METHOD_UNKNOWN";
constexpr char kErrRemoteTransport[] = "AUTH_PROXY_ASSERTION_REMOTE_TRANSPORT";
constexpr char kErrMissingPayload[] = "AUTH_PROXY_ASSERTION_PAYLOAD_MISSING";
constexpr char kErrOversizedPayload[] = "AUTH_PROXY_ASSERTION_PAYLOAD_OVERSIZED";
constexpr char kErrMalformedPayload[] = "AUTH_PROXY_ASSERTION_PAYLOAD_MALFORMED";
constexpr char kErrResolverUnavailable[] = "AUTH_PROXY_ASSERTION_RESOLVER_UNAVAILABLE";
constexpr char kErrSubjectUnknown[] = "AUTH_PROXY_ASSERTION_SUBJECT_UNKNOWN";
constexpr char kErrNoContinuation[] = "AUTH_PROXY_ASSERTION_NO_CONTINUE";
constexpr char kErrSigningKeyUnavailable[] = "AUTH_PROXY_ASSERTION_SIGNING_KEY_UNAVAILABLE";
constexpr char kErrUntrustedProxy[] = "AUTH_PROXY_ASSERTION_UNTRUSTED_PROXY";
constexpr char kErrIssuerMismatch[] = "AUTH_PROXY_ASSERTION_ISSUER_MISMATCH";
constexpr char kErrAudienceMismatch[] = "AUTH_PROXY_ASSERTION_AUDIENCE_MISMATCH";
constexpr char kErrProxyIdMismatch[] = "AUTH_PROXY_ASSERTION_PROXY_ID_MISMATCH";
constexpr char kErrExpired[] = "AUTH_PROXY_ASSERTION_EXPIRED";
constexpr char kErrSignatureInvalid[] = "AUTH_PROXY_ASSERTION_SIGNATURE_INVALID";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};
scratchbird::security::plugins::observability::OutcomeCounters g_counters{};
thread_local std::string g_health_payload;
thread_local std::string g_external_subject_payload;

struct ParsedProxyAssertion {
    std::string issuer;
    std::string subject;
    std::string audience;
    std::string proxy_id;
    uint64_t exp_unix_ms = 0;
    std::string signature;
};

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

inline bool payloadLooksSafe(sb_auth_slice_t payload) {
    if (!payload.ptr || payload.len == 0 || payload.len > kMaxPayloadBytes) {
        return false;
    }

    for (uint32_t i = 0; i < payload.len; ++i) {
        const uint8_t c = payload.ptr[i];
        const bool allowed =
            (c >= static_cast<uint8_t>('a') && c <= static_cast<uint8_t>('z')) ||
            (c >= static_cast<uint8_t>('A') && c <= static_cast<uint8_t>('Z')) ||
            (c >= static_cast<uint8_t>('0') && c <= static_cast<uint8_t>('9')) ||
            c == static_cast<uint8_t>(':') || c == static_cast<uint8_t>('=') ||
            c == static_cast<uint8_t>(';') || c == static_cast<uint8_t>('_') ||
            c == static_cast<uint8_t>('-') || c == static_cast<uint8_t>('.') ||
            c == static_cast<uint8_t>(',');
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

inline uint64_t nowUnixMs() {
    if (g_host_api && g_host_api->now_unix_ms) {
        return g_host_api->now_unix_ms();
    }
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

inline bool parseUInt64(std::string_view text, uint64_t* out_value) {
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

inline bool parseBool(std::string_view text, bool* out_value) {
    if (!out_value) {
        return false;
    }
    if (text == "1" || text == "true" || text == "yes" || text == "on") {
        *out_value = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "no" || text == "off") {
        *out_value = false;
        return true;
    }
    return false;
}

inline bool isLowerHex(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

inline bool isTokenFieldChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '_' || ch == '-' || ch == '.' || ch == ':' || ch == '/' || ch == '@';
}

inline bool parseBoundedField(std::string_view value,
                              uint32_t max_len,
                              bool (*allowed_char)(char)) {
    if (!allowed_char || value.empty() || value.size() > max_len) {
        return false;
    }
    for (char ch : value) {
        if (!allowed_char(ch)) {
            return false;
        }
    }
    return true;
}

inline bool parseAssertion(std::string_view payload, ParsedProxyAssertion* out_assertion) {
    if (!out_assertion || payload.empty()) {
        return false;
    }

    ParsedProxyAssertion parsed{};
    bool issuer_seen = false;
    bool subject_seen = false;
    bool audience_seen = false;
    bool proxy_seen = false;
    bool exp_seen = false;
    bool signature_seen = false;

    std::size_t cursor = 0;
    while (cursor < payload.size()) {
        const std::size_t sep = payload.find(';', cursor);
        const std::size_t end = (sep == std::string_view::npos) ? payload.size() : sep;
        if (end <= cursor) {
            return false;
        }

        const std::string_view pair = payload.substr(cursor, end - cursor);
        const std::size_t eq = pair.find('=');
        if (eq == std::string_view::npos || eq == 0 || eq + 1 >= pair.size()) {
            return false;
        }

        const std::string_view key = pair.substr(0, eq);
        const std::string_view value = pair.substr(eq + 1);

        if (key == "iss") {
            if (issuer_seen || !parseBoundedField(value, kMaxIssuerBytes, &isTokenFieldChar)) {
                return false;
            }
            parsed.issuer.assign(value.begin(), value.end());
            issuer_seen = true;
        } else if (key == "sub") {
            if (subject_seen || !parseBoundedField(value, kMaxSubjectBytes, &isTokenFieldChar)) {
                return false;
            }
            parsed.subject.assign(value.begin(), value.end());
            subject_seen = true;
        } else if (key == "aud") {
            if (audience_seen || !parseBoundedField(value, kMaxAudienceBytes, &isTokenFieldChar)) {
                return false;
            }
            parsed.audience.assign(value.begin(), value.end());
            audience_seen = true;
        } else if (key == "proxy") {
            if (proxy_seen || !parseBoundedField(value, kMaxProxyIdBytes, &isTokenFieldChar)) {
                return false;
            }
            parsed.proxy_id.assign(value.begin(), value.end());
            proxy_seen = true;
        } else if (key == "exp") {
            if (exp_seen) {
                return false;
            }
            if (!parseUInt64(value, &parsed.exp_unix_ms)) {
                return false;
            }
            exp_seen = true;
        } else if (key == "sig") {
            if (signature_seen || value.size() != kExpectedSignatureHexBytes) {
                return false;
            }
            for (char ch : value) {
                if (!isLowerHex(ch)) {
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

    if (!issuer_seen || !subject_seen || !audience_seen || !proxy_seen || !exp_seen ||
        !signature_seen || parsed.exp_unix_ms == 0) {
        return false;
    }

    *out_assertion = std::move(parsed);
    return true;
}

inline std::string computeSignature(const ParsedProxyAssertion& assertion,
                                    const std::string& signing_key) {
    const std::string payload = assertion.issuer + "|" + assertion.subject + "|" +
                                assertion.audience + "|" + assertion.proxy_id + "|" +
                                std::to_string(assertion.exp_unix_ms);
    std::string signature;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(signing_key, payload, &signature)) {
        return "";
    }
    return signature;
}

inline bool signaturesEqual(std::string_view lhs, std::string_view rhs) {
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

inline bool loadPolicyString(const char* key, std::string* out_value) {
    if (!key || !out_value || !g_host_api || !g_host_api->read_policy_value) {
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

    *out_value = toString(value);
    return !out_value->empty();
}

inline bool loadPolicyUInt64(const char* key, uint64_t* out_value) {
    if (!out_value) {
        return false;
    }
    std::string value;
    if (!loadPolicyString(key, &value)) {
        return false;
    }
    return parseUInt64(value, out_value);
}

inline bool loadPolicyBool(const char* key, bool fallback) {
    std::string value;
    if (!loadPolicyString(key, &value)) {
        return fallback;
    }

    bool parsed = fallback;
    if (!parseBool(value, &parsed)) {
        return fallback;
    }
    return parsed;
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
            "auth_plugin.proxy_assertion.begin",
            kPluginId,
            kMethodProxyPrincipalAssertion,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result || !conn) {
        fillDeniedResult(out_result, 9001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }
    *inout_exchange = 0;

    if (!sliceEqualsLiteral(method_id, kMethodProxyPrincipalAssertion)) {
        fillDeniedResult(out_result, 9002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, kErrUnknownMethod);
    }

    const bool allow_inet = loadPolicyBool(kPolicyAllowInet, false);
    if (conn->transport == SB_AUTH_TRANSPORT_INET && !allow_inet) {
        fillDeniedResult(out_result, 9003, kErrRemoteTransport);
        return finish(SB_AUTH_RC_DENY, kErrRemoteTransport);
    }

    uint64_t trusted_proxy_uid = 0;
    if (!loadPolicyUInt64(kPolicyTrustedProxyUid, &trusted_proxy_uid)) {
        fillDeniedResult(out_result, 9011, kErrUntrustedProxy);
        return finish(SB_AUTH_RC_DENY, kErrUntrustedProxy);
    }
    if (static_cast<uint64_t>(conn->peer_uid) != trusted_proxy_uid || conn->peer_pid == 0) {
        fillDeniedResult(out_result, 9011, kErrUntrustedProxy);
        return finish(SB_AUTH_RC_DENY, kErrUntrustedProxy);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 9004, kErrMissingPayload);
        return finish(SB_AUTH_RC_DENY, kErrMissingPayload);
    }
    if (client_payload.len > kMaxPayloadBytes) {
        fillDeniedResult(out_result, 9005, kErrOversizedPayload);
        return finish(SB_AUTH_RC_DENY, kErrOversizedPayload);
    }
    if (!payloadLooksSafe(client_payload)) {
        fillDeniedResult(out_result, 9006, kErrMalformedPayload);
        return finish(SB_AUTH_RC_DENY, kErrMalformedPayload);
    }

    ParsedProxyAssertion assertion{};
    if (!parseAssertion(toString(client_payload), &assertion)) {
        fillDeniedResult(out_result, 9006, kErrMalformedPayload);
        return finish(SB_AUTH_RC_DENY, kErrMalformedPayload);
    }

    scratchbird::security::plugins::secrets::SecretMaterial signing_key{};
    if (scratchbird::security::plugins::secrets::resolveSecretReference(
            g_host_api,
            kSigningKeyPolicyRef,
            &signing_key) != scratchbird::security::plugins::secrets::SecretResolveStatus::OK) {
        fillDeniedResult(out_result, 9010, kErrSigningKeyUnavailable);
        return finish(SB_AUTH_RC_DENY, kErrSigningKeyUnavailable);
    }

    std::string expected_issuer = kDefaultIssuer;
    std::string required_audience;
    std::string expected_proxy_id;
    (void) loadPolicyString(kPolicyExpectedIssuer, &expected_issuer);
    (void) loadPolicyString(kPolicyRequiredAudience, &required_audience);
    (void) loadPolicyString(kPolicyExpectedProxyId, &expected_proxy_id);

    if (assertion.issuer != expected_issuer) {
        fillDeniedResult(out_result, 9012, kErrIssuerMismatch);
        return finish(SB_AUTH_RC_DENY, kErrIssuerMismatch);
    }
    if (!required_audience.empty() && assertion.audience != required_audience) {
        fillDeniedResult(out_result, 9013, kErrAudienceMismatch);
        return finish(SB_AUTH_RC_DENY, kErrAudienceMismatch);
    }
    if (!expected_proxy_id.empty() && assertion.proxy_id != expected_proxy_id) {
        fillDeniedResult(out_result, 9014, kErrProxyIdMismatch);
        return finish(SB_AUTH_RC_DENY, kErrProxyIdMismatch);
    }
    if (assertion.exp_unix_ms <= nowUnixMs()) {
        fillDeniedResult(out_result, 9015, kErrExpired);
        return finish(SB_AUTH_RC_DENY, kErrExpired);
    }

    const std::string expected_signature = computeSignature(assertion, signing_key.value);
    if (!signaturesEqual(expected_signature, assertion.signature)) {
        fillDeniedResult(out_result, 9016, kErrSignatureInvalid);
        out_result->rc = SB_AUTH_RC_SIGNATURE_INVALID;
        return finish(SB_AUTH_RC_SIGNATURE_INVALID, kErrSignatureInvalid);
    }

    if (!g_host_api || !g_host_api->resolve_user_by_external_subject) {
        fillDeniedResult(out_result, 9007, kErrResolverUnavailable);
        return finish(SB_AUTH_RC_DENY, kErrResolverUnavailable);
    }

    const sb_auth_slice_t issuer{
        reinterpret_cast<const uint8_t*>(assertion.issuer.data()),
        static_cast<uint32_t>(assertion.issuer.size())
    };
    const sb_auth_slice_t subject{
        reinterpret_cast<const uint8_t*>(assertion.subject.data()),
        static_cast<uint32_t>(assertion.subject.size())
    };

    zeroAndPrimeResult(out_result);
    const sb_auth_rc_t resolve_rc = g_host_api->resolve_user_by_external_subject(
        issuer,
        subject,
        out_result->principal.principal_uuid);
    if (resolve_rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result, 9008, kErrSubjectUnknown);
        return finish(SB_AUTH_RC_DENY, kErrSubjectUnknown);
    }

    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 90;
    g_external_subject_payload = assertion.subject;
    out_result->principal.external_subject = sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(g_external_subject_payload.data()),
        static_cast<uint32_t>(g_external_subject_payload.size())
    };
    if (conn->username.ptr && conn->username.len > 0) {
        out_result->principal.resolved_username = conn->username;
    }
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
    return finish(SB_AUTH_RC_OK, nullptr);
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t /*exchange*/,
                                 sb_auth_slice_t /*client_payload*/,
                                 sb_auth_step_result_v1* out_result) {
    fillDeniedResult(out_result, 9009, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.proxy_assertion.continue",
        kPluginId,
        kMethodProxyPrincipalAssertion,
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
    g_health_payload = "{\"status\":\"ok\",\"plugin\":\"proxy_assertion\",\"allow_count\":" +
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
        copyCStr(methods[0].method_id,
                 sizeof(methods[0].method_id),
                 kMethodProxyPrincipalAssertion);
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
