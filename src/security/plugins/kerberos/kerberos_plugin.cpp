/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "kerberos_plugin_config.h"
#include "../auth_plugin_observability.h"
#include "../auth_plugin_crypto.h"
#include "../auth_plugin_secret_provider.h"
#include "../challenge_state_store.h"
#include "../enterprise_config_parser.h"

#include "scratchbird/security/auth_plugin_abi_v1.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.kerberos";
constexpr char kPluginVersion[] = "2.1.0";
constexpr char kMethodKerberosGssApi[] = "scratchbird.auth.kerberos_gssapi";
constexpr char kIssuerKerberos[] = "kerberos";
constexpr char kKerberosProviderDriverNative[] = "kerberos_native";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxPayloadBytes = 4096;
constexpr uint32_t kMaxPrincipalBytes = 256;
constexpr uint32_t kMaxSubjectBytes = 256;
constexpr uint32_t kMaxNonceBytes = 128;
constexpr uint32_t kMaxKdcEndpointBytes = 128;
constexpr uint32_t kExpectedSignatureHexBytes = 64;

constexpr char kErrBadRequest[] = "AUTH_KERBEROS_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_KERBEROS_METHOD_UNKNOWN";
constexpr char kErrMissingPayload[] = "AUTH_KERBEROS_PAYLOAD_MISSING";
constexpr char kErrMalformedPayload[] = "AUTH_KERBEROS_PAYLOAD_MALFORMED";
constexpr char kErrConfigInvalid[] = "AUTH_KERBEROS_CONFIG_INVALID";
constexpr char kErrKeytabUnavailable[] = "AUTH_KERBEROS_KEYTAB_UNAVAILABLE";
constexpr char kErrEndpointDenied[] = "AUTH_KERBEROS_KDC_ENDPOINT_NOT_ALLOWED";
constexpr char kErrTicketInvalid[] = "AUTH_KERBEROS_TICKET_INVALID";
constexpr char kErrReplayDetected[] = "AUTH_KERBEROS_REPLAY_DETECTED";
constexpr char kErrTicketExpired[] = "AUTH_KERBEROS_TICKET_EXPIRED";
constexpr char kErrResolverUnavailable[] = "AUTH_KERBEROS_RESOLVER_UNAVAILABLE";
constexpr char kErrSubjectUnknown[] = "AUTH_KERBEROS_SUBJECT_UNKNOWN";
constexpr char kErrNoContinuation[] = "AUTH_KERBEROS_NO_CONTINUE";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};

struct InstanceState {
    bool configured = false;
    scratchbird::security::plugins::kerberos::KerberosPluginConfig config;
    std::string config_error;
};

struct KerberosTicket {
    std::string service_principal;
    std::string subject;
    uint64_t issued_unix_ms = 0;
    std::string nonce;
    std::string kdc_endpoint;
    std::string signature;
};

struct KerberosProviderResult {
    sb_auth_rc_t rc = SB_AUTH_RC_DENY;
    uint32_t plugin_error_numeric = 14010;
    const char* plugin_error_code = kErrTicketInvalid;
    std::string subject;
};

std::mutex g_instances_mutex;
std::unordered_map<sb_auth_plugin_instance_t, InstanceState> g_instances;
scratchbird::security::plugins::observability::OutcomeCounters g_counters{};
thread_local std::string g_health_payload;
thread_local std::string g_external_subject_payload;
scratchbird::security::plugins::challenge::ReplayCache g_replay_cache{30000};

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

inline std::string toString(sb_auth_slice_t slice) {
    if (!slice.ptr || slice.len == 0) {
        return "";
    }
    const char* begin = reinterpret_cast<const char*>(slice.ptr);
    return std::string(begin, begin + slice.len);
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

    uint64_t value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        const uint64_t digit = static_cast<uint64_t>(ch - '0');
        const uint64_t next = value * 10u + digit;
        if (next < value) {
            return false;
        }
        value = next;
    }
    *out_value = value;
    return true;
}

inline bool isHexLower(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

inline bool isTicketPrincipalChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '-' || ch == '_' || ch == '.' || ch == ':' ||
           ch == '/' || ch == '@';
}

inline bool isTicketSubjectChar(char ch) {
    return isTicketPrincipalChar(ch);
}

inline bool isTicketNonceChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '-' || ch == '_' || ch == '.' || ch == ':';
}

inline bool isKdcEndpointChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '-' || ch == '_' || ch == '.' || ch == ':';
}

inline bool parsePrincipalField(std::string_view value, std::string* out_principal) {
    if (!out_principal || value.empty() || value.size() > kMaxPrincipalBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isTicketPrincipalChar(ch)) {
            return false;
        }
    }
    out_principal->assign(value.begin(), value.end());
    return true;
}

inline bool parseSubjectField(std::string_view value, std::string* out_subject) {
    if (!out_subject || value.empty() || value.size() > kMaxSubjectBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isTicketSubjectChar(ch)) {
            return false;
        }
    }
    out_subject->assign(value.begin(), value.end());
    return true;
}

inline bool parseNonceField(std::string_view value, std::string* out_nonce) {
    if (!out_nonce || value.empty() || value.size() > kMaxNonceBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isTicketNonceChar(ch)) {
            return false;
        }
    }
    out_nonce->assign(value.begin(), value.end());
    return true;
}

inline bool parseKdcEndpointField(std::string_view value, std::string* out_kdc_endpoint) {
    if (!out_kdc_endpoint || value.empty() || value.size() > kMaxKdcEndpointBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isKdcEndpointChar(ch)) {
            return false;
        }
    }
    out_kdc_endpoint->assign(value.begin(), value.end());
    return true;
}

inline bool parseSignatureField(std::string_view value, std::string* out_signature) {
    if (!out_signature || value.size() != kExpectedSignatureHexBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isHexLower(ch)) {
            return false;
        }
    }
    out_signature->assign(value.begin(), value.end());
    return true;
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
            c == static_cast<uint8_t>('_') || c == static_cast<uint8_t>('-') ||
            c == static_cast<uint8_t>('.') || c == static_cast<uint8_t>(':') ||
            c == static_cast<uint8_t>('=') || c == static_cast<uint8_t>(';') ||
            c == static_cast<uint8_t>('/') || c == static_cast<uint8_t>('@');
        if (!allowed) {
            return false;
        }
    }

    return true;
}

inline bool parseTicket(sb_auth_slice_t payload, KerberosTicket* out_ticket) {
    if (!out_ticket || !payloadLooksSafe(payload)) {
        return false;
    }

    const std::string payload_text = toString(payload);
    if (payload_text.empty()) {
        return false;
    }

    KerberosTicket parsed{};
    bool principal_seen = false;
    bool subject_seen = false;
    bool timestamp_seen = false;
    bool nonce_seen = false;
    bool kdc_seen = false;
    bool signature_seen = false;
    std::size_t cursor = 0;
    while (cursor < payload_text.size()) {
        const std::size_t sep = payload_text.find(';', cursor);
        const std::size_t end = (sep == std::string::npos) ? payload_text.size() : sep;
        if (end <= cursor) {
            return false;
        }

        const std::string_view pair(payload_text.data() + cursor, end - cursor);
        const std::size_t eq = pair.find('=');
        if (eq == std::string_view::npos || eq == 0 || eq + 1 >= pair.size()) {
            return false;
        }

        const std::string_view key = pair.substr(0, eq);
        const std::string_view value = pair.substr(eq + 1);
        if (key == "princ" || key == "principal") {
            if (principal_seen || !parsePrincipalField(value, &parsed.service_principal)) {
                return false;
            }
            principal_seen = true;
        } else if (key == "sub" || key == "subject") {
            if (subject_seen || !parseSubjectField(value, &parsed.subject)) {
                return false;
            }
            subject_seen = true;
        } else if (key == "ts") {
            if (timestamp_seen || !parseUInt64(value, &parsed.issued_unix_ms)) {
                return false;
            }
            timestamp_seen = true;
        } else if (key == "nonce") {
            if (nonce_seen || !parseNonceField(value, &parsed.nonce)) {
                return false;
            }
            nonce_seen = true;
        } else if (key == "kdc") {
            if (kdc_seen || !parseKdcEndpointField(value, &parsed.kdc_endpoint)) {
                return false;
            }
            kdc_seen = true;
        } else if (key == "sig") {
            if (signature_seen || !parseSignatureField(value, &parsed.signature)) {
                return false;
            }
            signature_seen = true;
        } else {
            return false;
        }

        if (sep == std::string::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (!principal_seen || !subject_seen || !timestamp_seen || !nonce_seen ||
        !signature_seen || parsed.issued_unix_ms == 0) {
        return false;
    }

    *out_ticket = std::move(parsed);
    return true;
}

inline std::string computeTicketSignature(const KerberosTicket& ticket,
                                          const std::string& keytab_secret) {
    const std::string payload = ticket.service_principal + "|" + ticket.subject + "|" +
                                std::to_string(ticket.issued_unix_ms) + "|" + ticket.nonce +
                                "|" + ticket.kdc_endpoint;
    std::string signature;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(keytab_secret,
                                                                payload,
                                                                &signature)) {
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

inline bool isTestRuntimeProfile(
    scratchbird::security::plugins::kerberos::KerberosRuntimeProfile profile) {
    return profile == scratchbird::security::plugins::kerberos::KerberosRuntimeProfile::TEST;
}

inline const char* runtimeProfileName(
    scratchbird::security::plugins::kerberos::KerberosRuntimeProfile profile) {
    return isTestRuntimeProfile(profile) ? "test" : "production";
}

inline KerberosProviderResult evaluateKerberosProviderDecision(
    const InstanceState& state,
    const sb_auth_connection_ctx_v1* conn,
    const KerberosTicket& ticket) {
    KerberosProviderResult result{};

    if (ticket.service_principal != state.config.service_principal) {
        result.plugin_error_numeric = 14010;
        result.plugin_error_code = kErrTicketInvalid;
        return result;
    }

    const std::string endpoint = ticket.kdc_endpoint.empty() ? toString(conn->server_address)
                                                              : ticket.kdc_endpoint;
    if (!state.config.allowed_kdc_endpoints.empty()) {
        const auto allowed = std::find(state.config.allowed_kdc_endpoints.begin(),
                                       state.config.allowed_kdc_endpoints.end(),
                                       endpoint);
        if (allowed == state.config.allowed_kdc_endpoints.end()) {
            result.plugin_error_numeric = 14005;
            result.plugin_error_code = kErrEndpointDenied;
            return result;
        }
    }

    scratchbird::security::plugins::secrets::SecretMaterial keytab_material{};
    if (scratchbird::security::plugins::secrets::resolveSecretReference(
            g_host_api,
            state.config.keytab_path,
            &keytab_material) != scratchbird::security::plugins::secrets::SecretResolveStatus::OK) {
        result.plugin_error_numeric = 14008;
        result.plugin_error_code = kErrKeytabUnavailable;
        return result;
    }

    const uint64_t now_unix_ms = nowUnixMs();
    const uint64_t replay_window_ms =
        state.config.max_replay_window_ms == 0 ? 30000u : state.config.max_replay_window_ms;

    if (ticket.issued_unix_ms > now_unix_ms + replay_window_ms ||
        now_unix_ms > ticket.issued_unix_ms + replay_window_ms) {
        result.plugin_error_numeric = 14011;
        result.plugin_error_code = kErrTicketExpired;
        return result;
    }

    const std::string expected_signature = computeTicketSignature(ticket, keytab_material.value);
    if (!signaturesEqual(expected_signature, ticket.signature)) {
        result.plugin_error_numeric = 14010;
        result.plugin_error_code = kErrTicketInvalid;
        return result;
    }

    g_replay_cache.setTtlMs(replay_window_ms);
    const std::string replay_key = ticket.service_principal + "|" + ticket.nonce;
    if (!g_replay_cache.remember(replay_key)) {
        result.plugin_error_numeric = 14012;
        result.plugin_error_code = kErrReplayDetected;
        return result;
    }

    result.rc = SB_AUTH_RC_OK;
    result.plugin_error_numeric = 0;
    result.plugin_error_code = nullptr;
    result.subject = ticket.subject;
    return result;
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

    const sb_auth_plugin_instance_t instance =
        g_next_instance.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        g_instances[instance] = InstanceState{};
    }

    *out_instance = instance;
    return SB_AUTH_RC_OK;
}

inline void destroyInstance(sb_auth_plugin_instance_t instance) {
    std::lock_guard<std::mutex> lock(g_instances_mutex);
    g_instances.erase(instance);
}

inline sb_auth_rc_t configureInstance(sb_auth_plugin_instance_t instance,
                                      sb_auth_slice_t config_json) {
    std::string parse_error;
    const auto values = scratchbird::security::plugins::enterprise::parseFlatConfig(
        config_json,
        &parse_error);

    scratchbird::security::plugins::kerberos::KerberosPluginConfig parsed;
    std::string config_error;
    const auto status = scratchbird::security::plugins::kerberos::loadKerberosPluginConfig(
        values,
        parsed,
        &config_error);

    std::lock_guard<std::mutex> lock(g_instances_mutex);
    auto it = g_instances.find(instance);
    if (it == g_instances.end()) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }

    if (status != scratchbird::security::plugins::kerberos::KerberosPluginConfigStatus::OK) {
        it->second.configured = false;
        it->second.config_error = config_error.empty() ? parse_error : config_error;
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }

    it->second.config = std::move(parsed);
    it->second.configured = true;
    it->second.config_error.clear();
    return SB_AUTH_RC_OK;
}

inline sb_auth_rc_t beginAuth(sb_auth_plugin_instance_t instance,
                              sb_auth_slice_t method_id,
                              const sb_auth_connection_ctx_v1* conn,
                              sb_auth_slice_t client_payload,
                              sb_auth_exchange_t* inout_exchange,
                              sb_auth_step_result_v1* out_result) {
    auto finish = [&](sb_auth_rc_t rc, const char* error_code) {
        scratchbird::security::plugins::observability::recordOutcome(g_counters, rc);
        scratchbird::security::plugins::observability::emitAuditEvent(
            g_host_api,
            "auth_plugin.kerberos.begin",
            kPluginId,
            kMethodKerberosGssApi,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result || !conn) {
        fillDeniedResult(out_result, 14001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }
    *inout_exchange = 0;

    if (!sliceEqualsLiteral(method_id, kMethodKerberosGssApi)) {
        fillDeniedResult(out_result, 14002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 14003, kErrMissingPayload);
        return finish(SB_AUTH_RC_DENY, kErrMissingPayload);
    }

    KerberosTicket ticket{};
    if (!parseTicket(client_payload, &ticket)) {
        fillDeniedResult(out_result, 14009, kErrMalformedPayload);
        return finish(SB_AUTH_RC_DENY, kErrMalformedPayload);
    }

    InstanceState state;
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        auto it = g_instances.find(instance);
        if (it == g_instances.end() || !it->second.configured) {
            fillDeniedResult(out_result, 14004, kErrConfigInvalid);
            return finish(SB_AUTH_RC_DENY, kErrConfigInvalid);
        }
        state = it->second;
    }

    const KerberosProviderResult provider_result =
        evaluateKerberosProviderDecision(state, conn, ticket);
    if (provider_result.rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result,
                         provider_result.plugin_error_numeric,
                         provider_result.plugin_error_code);
        return finish(provider_result.rc, provider_result.plugin_error_code);
    }

    if (!g_host_api || !g_host_api->resolve_user_by_external_subject) {
        fillDeniedResult(out_result, 14013, kErrResolverUnavailable);
        return finish(SB_AUTH_RC_DENY, kErrResolverUnavailable);
    }

    const sb_auth_slice_t issuer{
        reinterpret_cast<const uint8_t*>(kIssuerKerberos),
        static_cast<uint32_t>(sizeof(kIssuerKerberos) - 1)
    };
    const sb_auth_slice_t subject{
        reinterpret_cast<const uint8_t*>(provider_result.subject.data()),
        static_cast<uint32_t>(provider_result.subject.size())
    };

    zeroAndPrimeResult(out_result);
    const sb_auth_rc_t resolve_rc = g_host_api->resolve_user_by_external_subject(
        issuer,
        subject,
        out_result->principal.principal_uuid);
    if (resolve_rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result, 14014, kErrSubjectUnknown);
        return finish(SB_AUTH_RC_DENY, kErrSubjectUnknown);
    }

    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 55;
    g_external_subject_payload = provider_result.subject;
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
    fillDeniedResult(out_result, 14007, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.kerberos.continue",
        kPluginId,
        kMethodKerberosGssApi,
        SB_AUTH_RC_DENY,
        kErrNoContinuation);
    return SB_AUTH_RC_DENY;
}

inline void abortAuth(sb_auth_plugin_instance_t /*instance*/,
                      sb_auth_exchange_t /*exchange*/) {}

inline sb_auth_rc_t healthCheck(sb_auth_plugin_instance_t instance,
                                sb_auth_slice_t* out_json) {
    if (!out_json) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }

    bool configured = false;
    scratchbird::security::plugins::kerberos::KerberosRuntimeProfile runtime_profile =
        scratchbird::security::plugins::kerberos::KerberosRuntimeProfile::PRODUCTION;
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        auto it = g_instances.find(instance);
        configured = (it != g_instances.end() && it->second.configured);
        if (it != g_instances.end()) {
            runtime_profile = it->second.config.runtime_profile;
        }
    }

    g_health_payload =
        "{\"status\":\"ok\",\"plugin\":\"kerberos\",\"configured\":" +
        std::string(configured ? "true" : "false") +
        ",\"runtime_profile\":\"" + runtimeProfileName(runtime_profile) + "\"" +
        ",\"provider_driver\":\"" + std::string(kKerberosProviderDriverNative) + "\"" +
        ",\"allow_count\":" +
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
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodKerberosGssApi);
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
