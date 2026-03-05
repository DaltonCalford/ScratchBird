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

#include "scratchbird/security/auth_plugin_abi_v1.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.certificate_mtls";
constexpr char kPluginVersion[] = "1.0.0";
constexpr char kMethodCertificateX509[] = "scratchbird.auth.certificate_x509";
constexpr char kCertIssuer[] = "x509";
constexpr char kPolicyRequiredSanPrefix[] = "auth.certificate_mtls.required_san_prefix";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxCertificateSubjectBytes = 8192;
constexpr uint32_t kMaxSubjectFieldBytes = 2048;
constexpr uint32_t kMaxSanFieldBytes = 512;

constexpr char kErrBadRequest[] = "AUTH_CERT_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_CERT_METHOD_UNKNOWN";
constexpr char kErrMissingSubject[] = "AUTH_CERT_SUBJECT_MISSING";
constexpr char kErrOversizedSubject[] = "AUTH_CERT_SUBJECT_OVERSIZED";
constexpr char kErrMalformedSubject[] = "AUTH_CERT_SUBJECT_MALFORMED";
constexpr char kErrResolverUnavailable[] = "AUTH_CERT_RESOLVER_UNAVAILABLE";
constexpr char kErrSubjectUnknown[] = "AUTH_CERT_SUBJECT_UNKNOWN";
constexpr char kErrChainUntrusted[] = "AUTH_CERT_CHAIN_UNTRUSTED";
constexpr char kErrRevoked[] = "AUTH_CERT_REVOKED";
constexpr char kErrSanPolicyMismatch[] = "AUTH_CERT_SAN_POLICY_MISMATCH";
constexpr char kErrNoContinuation[] = "AUTH_CERT_NO_CONTINUE";

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

inline bool subjectLooksSafe(sb_auth_slice_t subject) {
    if (!subject.ptr || subject.len == 0 || subject.len > kMaxCertificateSubjectBytes) {
        return false;
    }

    for (uint32_t i = 0; i < subject.len; ++i) {
        const uint8_t c = subject.ptr[i];
        if (c < 0x20 || c > 0x7E) {
            return false;
        }
    }

    return true;
}

struct ParsedCertificateAssertion {
    std::string subject;
    std::string san;
    bool chain_trusted = false;
    bool revoked = true;
};

inline std::string toString(sb_auth_slice_t value) {
    if (!value.ptr || value.len == 0) {
        return "";
    }
    const char* begin = reinterpret_cast<const char*>(value.ptr);
    return std::string(begin, begin + value.len);
}

inline bool parseBoolString(std::string_view value, bool* out_bool) {
    if (!out_bool) {
        return false;
    }

    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        *out_bool = true;
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        *out_bool = false;
        return true;
    }
    return false;
}

inline bool isSubjectFieldChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    switch (ch) {
        case ' ':
        case '=':
        case ',':
        case ':':
        case '.':
        case '-':
        case '_':
        case '/':
        case '@':
        case '+':
            return true;
        default:
            return false;
    }
}

inline bool isSanFieldChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    switch (ch) {
        case ':':
        case '.':
        case '-':
        case '_':
        case '/':
        case '@':
            return true;
        default:
            return false;
    }
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

inline bool parseCertificateAssertion(std::string_view payload, ParsedCertificateAssertion* out) {
    if (!out || payload.empty()) {
        return false;
    }

    ParsedCertificateAssertion parsed{};
    bool subject_seen = false;
    bool san_seen = false;
    bool chain_seen = false;
    bool revoked_seen = false;
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

        if (key == "sub") {
            if (subject_seen || !parseBoundedField(value, kMaxSubjectFieldBytes, &isSubjectFieldChar)) {
                return false;
            }
            parsed.subject.assign(value.begin(), value.end());
            subject_seen = true;
        } else if (key == "san") {
            if (san_seen || !parseBoundedField(value, kMaxSanFieldBytes, &isSanFieldChar)) {
                return false;
            }
            parsed.san.assign(value.begin(), value.end());
            san_seen = true;
        } else if (key == "chain") {
            if (chain_seen) {
                return false;
            }
            if (value == "trusted") {
                parsed.chain_trusted = true;
            } else if (value == "untrusted") {
                parsed.chain_trusted = false;
            } else {
                return false;
            }
            chain_seen = true;
        } else if (key == "revoked") {
            if (revoked_seen) {
                return false;
            }
            if (!parseBoolString(value, &parsed.revoked)) {
                return false;
            }
            revoked_seen = true;
        } else {
            return false;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (!subject_seen || !san_seen || !chain_seen || !revoked_seen) {
        return false;
    }
    *out = std::move(parsed);
    return true;
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
    *out = toString(value);
    return !out->empty();
}

inline bool startsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
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
            "auth_plugin.certificate_mtls.begin",
            kPluginId,
            kMethodCertificateX509,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result) {
        fillDeniedResult(out_result, 6001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }
    *inout_exchange = 0;

    if (!sliceEqualsLiteral(method_id, kMethodCertificateX509)) {
        fillDeniedResult(out_result, 6002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 6003, kErrMissingSubject);
        return finish(SB_AUTH_RC_DENY, kErrMissingSubject);
    }
    if (client_payload.len > kMaxCertificateSubjectBytes) {
        fillDeniedResult(out_result, 6004, kErrOversizedSubject);
        return finish(SB_AUTH_RC_DENY, kErrOversizedSubject);
    }
    if (!subjectLooksSafe(client_payload)) {
        fillDeniedResult(out_result, 6005, kErrMalformedSubject);
        return finish(SB_AUTH_RC_DENY, kErrMalformedSubject);
    }

    ParsedCertificateAssertion assertion{};
    if (!parseCertificateAssertion(toString(client_payload), &assertion)) {
        fillDeniedResult(out_result, 6005, kErrMalformedSubject);
        return finish(SB_AUTH_RC_DENY, kErrMalformedSubject);
    }

    if (!assertion.chain_trusted) {
        fillDeniedResult(out_result, 6009, kErrChainUntrusted);
        return finish(SB_AUTH_RC_DENY, kErrChainUntrusted);
    }
    if (assertion.revoked) {
        fillDeniedResult(out_result, 6010, kErrRevoked);
        return finish(SB_AUTH_RC_DENY, kErrRevoked);
    }

    std::string required_san_prefix;
    if (loadPolicyString(kPolicyRequiredSanPrefix, &required_san_prefix) &&
        !startsWith(assertion.san, required_san_prefix)) {
        fillDeniedResult(out_result, 6011, kErrSanPolicyMismatch);
        return finish(SB_AUTH_RC_DENY, kErrSanPolicyMismatch);
    }

    if (!g_host_api || !g_host_api->resolve_user_by_external_subject) {
        fillDeniedResult(out_result, 6006, kErrResolverUnavailable);
        return finish(SB_AUTH_RC_DENY, kErrResolverUnavailable);
    }

    const sb_auth_slice_t issuer{
        reinterpret_cast<const uint8_t*>(kCertIssuer),
        static_cast<uint32_t>(sizeof(kCertIssuer) - 1)
    };

    zeroAndPrimeResult(out_result);
    const sb_auth_rc_t resolve_rc = g_host_api->resolve_user_by_external_subject(
        issuer,
        sb_auth_slice_t{
            reinterpret_cast<const uint8_t*>(assertion.subject.data()),
            static_cast<uint32_t>(assertion.subject.size())
        },
        out_result->principal.principal_uuid);
    if (resolve_rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result, 6007, kErrSubjectUnknown);
        return finish(SB_AUTH_RC_DENY, kErrSubjectUnknown);
    }

    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 80;
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
    fillDeniedResult(out_result, 6008, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.certificate_mtls.continue",
        kPluginId,
        kMethodCertificateX509,
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
    g_health_payload = "{\"status\":\"ok\",\"plugin\":\"certificate_mtls\",\"allow_count\":" +
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
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodCertificateX509);
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
