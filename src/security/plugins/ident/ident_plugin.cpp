/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "ident_plugin_config.h"
#include "../auth_plugin_observability.h"
#include "../enterprise_config_parser.h"

#include "scratchbird/security/auth_plugin_abi_v1.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.ident";
constexpr char kPluginVersion[] = "2.1.0";
constexpr char kMethodIdentRfc1413[] = "scratchbird.auth.ident_rfc1413";
constexpr char kIdentProviderDriverNative[] = "ident_native";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxPayloadBytes = 1024;
constexpr uint32_t kMaxIdentUserBytes = 128;
constexpr uint32_t kMaxClientAddressBytes = 15;

constexpr char kErrBadRequest[] = "AUTH_IDENT_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_IDENT_METHOD_UNKNOWN";
constexpr char kErrQueryFailed[] = "AUTH_IDENT_QUERY_FAILED";
constexpr char kErrConfigInvalid[] = "AUTH_IDENT_CONFIG_INVALID";
constexpr char kErrUntrustedTransport[] = "AUTH_IDENT_UNTRUSTED_TRANSPORT";
constexpr char kErrUsernameMismatch[] = "AUTH_CREDENTIAL_INVALID";
constexpr char kErrAddressMismatch[] = "AUTH_IDENT_ADDRESS_MISMATCH";
constexpr char kErrUserUnknown[] = "AUTH_IDENT_USER_UNKNOWN";
constexpr char kErrNoContinuation[] = "AUTH_IDENT_NO_CONTINUE";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};

struct InstanceState {
    bool configured = false;
    scratchbird::security::plugins::ident::IdentPluginConfig config;
    std::string config_error;
};

struct IdentPayloadClaims {
    std::string ident_user;
    std::string claimed_client_addr;
    bool has_claimed_client_addr = false;
};

struct IdentProviderResult {
    sb_auth_rc_t rc = SB_AUTH_RC_DENY;
    uint32_t plugin_error_numeric = 15011;
    const char* plugin_error_code = kErrQueryFailed;
    std::string ident_user;
};

std::mutex g_instances_mutex;
std::unordered_map<sb_auth_plugin_instance_t, InstanceState> g_instances;
scratchbird::security::plugins::observability::OutcomeCounters g_counters{};
thread_local std::string g_health_payload;
thread_local std::string g_ident_user_payload;

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

inline bool sliceEqualsString(sb_auth_slice_t lhs, const std::string& rhs) {
    if (!lhs.ptr || lhs.len != rhs.size()) {
        return false;
    }
    return std::memcmp(lhs.ptr, rhs.data(), rhs.size()) == 0;
}

inline bool parseUInt8(std::string_view text, uint8_t* out_value) {
    if (!out_value || text.empty()) {
        return false;
    }

    uint32_t value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10u + static_cast<uint32_t>(ch - '0');
        if (value > 255u) {
            return false;
        }
    }

    *out_value = static_cast<uint8_t>(value);
    return true;
}

inline bool parseUInt32(std::string_view text, uint32_t* out_value) {
    if (!out_value || text.empty()) {
        return false;
    }

    uint64_t value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10u + static_cast<uint64_t>(ch - '0');
        if (value > 0xFFFFFFFFull) {
            return false;
        }
    }

    *out_value = static_cast<uint32_t>(value);
    return true;
}

inline bool parseIpv4(std::string_view text, uint32_t* out_addr) {
    if (!out_addr) {
        return false;
    }

    std::array<uint8_t, 4> octets{};
    std::size_t cursor = 0;
    for (std::size_t i = 0; i < octets.size(); ++i) {
        const std::size_t next = text.find('.', cursor);
        const bool last = (i == octets.size() - 1);
        if ((!last && next == std::string_view::npos) || (last && next != std::string_view::npos)) {
            return false;
        }

        const std::size_t end = last ? text.size() : next;
        if (end <= cursor) {
            return false;
        }

        uint8_t octet = 0;
        if (!parseUInt8(text.substr(cursor, end - cursor), &octet)) {
            return false;
        }
        octets[i] = octet;
        cursor = end + 1;
    }

    *out_addr = (static_cast<uint32_t>(octets[0]) << 24u) |
                (static_cast<uint32_t>(octets[1]) << 16u) |
                (static_cast<uint32_t>(octets[2]) << 8u) |
                static_cast<uint32_t>(octets[3]);
    return true;
}

inline bool parseCidr(std::string_view cidr, uint32_t* out_network, uint8_t* out_prefix_bits) {
    if (!out_network || !out_prefix_bits) {
        return false;
    }

    std::string_view ip_text = cidr;
    uint8_t prefix = 32;
    const std::size_t slash = cidr.find('/');
    if (slash != std::string_view::npos) {
        ip_text = cidr.substr(0, slash);
        uint32_t parsed_prefix = 0;
        if (!parseUInt32(cidr.substr(slash + 1), &parsed_prefix) || parsed_prefix > 32u) {
            return false;
        }
        prefix = static_cast<uint8_t>(parsed_prefix);
    }

    uint32_t ip = 0;
    if (!parseIpv4(ip_text, &ip)) {
        return false;
    }

    uint32_t mask = 0;
    if (prefix == 0u) {
        mask = 0u;
    } else {
        mask = 0xFFFFFFFFu << (32u - prefix);
    }

    *out_prefix_bits = prefix;
    *out_network = ip & mask;
    return true;
}

inline bool ipMatchesCidr(uint32_t ip, uint32_t network, uint8_t prefix_bits) {
    const uint32_t mask = (prefix_bits == 0u) ? 0u : (0xFFFFFFFFu << (32u - prefix_bits));
    return (ip & mask) == network;
}

inline bool trustedByCidrs(const std::string& address,
                           const std::vector<std::string>& cidrs) {
    uint32_t ip = 0;
    if (!parseIpv4(address, &ip)) {
        return false;
    }

    for (const auto& cidr : cidrs) {
        if (cidr.empty()) {
            continue;
        }
        uint32_t network = 0;
        uint8_t prefix = 0;
        if (!parseCidr(cidr, &network, &prefix)) {
            continue;
        }
        if (ipMatchesCidr(ip, network, prefix)) {
            return true;
        }
    }
    return false;
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
            c == static_cast<uint8_t>('=') || c == static_cast<uint8_t>(';');
        if (!allowed) {
            return false;
        }
    }

    return true;
}

inline bool isIdentUserChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '_' || ch == '-' || ch == '.' || ch == ':';
}

inline bool parseIdentUser(std::string_view value, std::string* out_ident_user) {
    if (!out_ident_user || value.empty() || value.size() > kMaxIdentUserBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isIdentUserChar(ch)) {
            return false;
        }
    }
    out_ident_user->assign(value.begin(), value.end());
    return true;
}

inline bool parseIdentPayload(sb_auth_slice_t payload, IdentPayloadClaims* out_claims) {
    if (!out_claims || !payloadLooksSafe(payload)) {
        return false;
    }

    const std::string payload_text = toString(payload);
    if (payload_text.empty()) {
        return false;
    }

    IdentPayloadClaims parsed{};
    bool ident_user_seen = false;
    bool client_addr_seen = false;

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
        if (key == "ident_user" || key == "user" || key == "ident") {
            if (ident_user_seen || !parseIdentUser(value, &parsed.ident_user)) {
                return false;
            }
            ident_user_seen = true;
        } else if (key == "client_addr") {
            if (client_addr_seen || value.size() > kMaxClientAddressBytes) {
                return false;
            }
            uint32_t ignored = 0;
            if (!parseIpv4(value, &ignored)) {
                return false;
            }
            parsed.claimed_client_addr.assign(value.begin(), value.end());
            parsed.has_claimed_client_addr = true;
            client_addr_seen = true;
        } else {
            return false;
        }

        if (sep == std::string::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (!ident_user_seen) {
        return false;
    }

    *out_claims = std::move(parsed);
    return true;
}

inline bool isTestRuntimeProfile(
    scratchbird::security::plugins::ident::IdentRuntimeProfile profile) {
    return profile == scratchbird::security::plugins::ident::IdentRuntimeProfile::TEST;
}

inline const char* runtimeProfileName(
    scratchbird::security::plugins::ident::IdentRuntimeProfile profile) {
    return isTestRuntimeProfile(profile) ? "test" : "production";
}

inline IdentProviderResult evaluateIdentProviderDecision(const InstanceState& state,
                                                         const sb_auth_connection_ctx_v1* conn,
                                                         sb_auth_slice_t client_payload) {
    IdentProviderResult result{};

    if (state.config.ident_timeout_ms == 0) {
        result.plugin_error_numeric = 15005;
        result.plugin_error_code = kErrQueryFailed;
        return result;
    }

    const std::string remote_address = toString(conn->client_address);
    if (!trustedByCidrs(remote_address, state.config.trusted_cidrs)) {
        result.plugin_error_numeric = 15006;
        result.plugin_error_code = kErrUntrustedTransport;
        return result;
    }

    IdentPayloadClaims claims{};
    if (!parseIdentPayload(client_payload, &claims)) {
        result.plugin_error_numeric = 15011;
        result.plugin_error_code = kErrQueryFailed;
        return result;
    }

    if (claims.has_claimed_client_addr && claims.claimed_client_addr != remote_address) {
        result.plugin_error_numeric = 15008;
        result.plugin_error_code = kErrAddressMismatch;
        return result;
    }

    if (state.config.require_username_match &&
        !sliceEqualsString(conn->username, claims.ident_user)) {
        result.plugin_error_numeric = 15007;
        result.plugin_error_code = kErrUsernameMismatch;
        return result;
    }

    result.rc = SB_AUTH_RC_OK;
    result.plugin_error_numeric = 0;
    result.plugin_error_code = nullptr;
    result.ident_user = claims.ident_user;
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

    scratchbird::security::plugins::ident::IdentPluginConfig parsed;
    std::string config_error;
    const auto status = scratchbird::security::plugins::ident::loadIdentPluginConfig(
        values,
        parsed,
        &config_error);

    std::lock_guard<std::mutex> lock(g_instances_mutex);
    auto it = g_instances.find(instance);
    if (it == g_instances.end()) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }

    if (status != scratchbird::security::plugins::ident::IdentPluginConfigStatus::OK) {
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
            "auth_plugin.ident.begin",
            kPluginId,
            kMethodIdentRfc1413,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result || !conn) {
        fillDeniedResult(out_result, 15001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }
    *inout_exchange = 0;

    if (!sliceEqualsLiteral(method_id, kMethodIdentRfc1413)) {
        fillDeniedResult(out_result, 15002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 15003, kErrQueryFailed);
        return finish(SB_AUTH_RC_DENY, kErrQueryFailed);
    }

    InstanceState state;
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        auto it = g_instances.find(instance);
        if (it == g_instances.end() || !it->second.configured) {
            fillDeniedResult(out_result, 15004, kErrConfigInvalid);
            return finish(SB_AUTH_RC_DENY, kErrConfigInvalid);
        }
        state = it->second;
    }

    const IdentProviderResult provider_result =
        evaluateIdentProviderDecision(state, conn, client_payload);
    if (provider_result.rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result,
                         provider_result.plugin_error_numeric,
                         provider_result.plugin_error_code);
        return finish(provider_result.rc, provider_result.plugin_error_code);
    }

    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 25;
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");

    g_ident_user_payload = provider_result.ident_user;
    out_result->principal.resolved_username = sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(g_ident_user_payload.data()),
        static_cast<uint32_t>(g_ident_user_payload.size())
    };

    if (g_host_api && g_host_api->resolve_user_by_name) {
        const sb_auth_rc_t resolve_rc = g_host_api->resolve_user_by_name(
            out_result->principal.resolved_username,
            out_result->principal.principal_uuid);
        if (resolve_rc != SB_AUTH_RC_OK) {
            fillDeniedResult(out_result, 15009, kErrUserUnknown);
            return finish(SB_AUTH_RC_DENY, kErrUserUnknown);
        }
    }

    return finish(SB_AUTH_RC_OK, nullptr);
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t /*exchange*/,
                                 sb_auth_slice_t /*client_payload*/,
                                 sb_auth_step_result_v1* out_result) {
    fillDeniedResult(out_result, 15010, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.ident.continue",
        kPluginId,
        kMethodIdentRfc1413,
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
    scratchbird::security::plugins::ident::IdentRuntimeProfile runtime_profile =
        scratchbird::security::plugins::ident::IdentRuntimeProfile::PRODUCTION;
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        auto it = g_instances.find(instance);
        configured = (it != g_instances.end() && it->second.configured);
        if (it != g_instances.end()) {
            runtime_profile = it->second.config.runtime_profile;
        }
    }

    g_health_payload = "{\"status\":\"ok\",\"plugin\":\"ident\",\"configured\":" +
                       std::string(configured ? "true" : "false") +
                       ",\"runtime_profile\":\"" + runtimeProfileName(runtime_profile) + "\"" +
                       ",\"provider_driver\":\"" + std::string(kIdentProviderDriverNative) + "\"" +
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
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodIdentRfc1413);
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
