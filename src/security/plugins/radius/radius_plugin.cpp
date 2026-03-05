/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "radius_plugin_config.h"
#include "../auth_plugin_observability.h"
#include "../auth_plugin_crypto.h"
#include "../auth_plugin_secret_provider.h"
#include "../enterprise_config_parser.h"

#include "scratchbird/security/auth_plugin_abi_v1.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.radius";
constexpr char kPluginVersion[] = "2.1.0";
constexpr char kMethodRadiusPap[] = "scratchbird.auth.radius_pap";
constexpr char kPolicyAllowTestDirectives[] = "auth.radius.allow_test_directives";
constexpr char kRadiusProviderDriverNative[] = "radius_native";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxPayloadBytes = 2048;
constexpr uint32_t kMaxPasswordBytes = 256;
constexpr uint32_t kExpectedAuthenticatorHexBytes = 64;
constexpr uint32_t kMaxDirectiveCount = 8;
constexpr uint32_t kMaxDirectiveBytes = 128;
constexpr uint32_t kMaxDirectiveTargetBytes = 64;

constexpr char kErrBadRequest[] = "AUTH_RADIUS_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_RADIUS_METHOD_UNKNOWN";
constexpr char kErrMissingPayload[] = "AUTH_RADIUS_PAYLOAD_MISSING";
constexpr char kErrMalformedPayload[] = "AUTH_RADIUS_PAYLOAD_MALFORMED";
constexpr char kErrConfigInvalid[] = "AUTH_RADIUS_CONFIG_INVALID";
constexpr char kErrTimeout[] = "AUTH_RADIUS_TIMEOUT";
constexpr char kErrSecretInvalid[] = "AUTH_RADIUS_SHARED_SECRET_INVALID";
constexpr char kErrPolicyDenied[] = "AUTH_PLUGIN_POLICY_DENIED";
constexpr char kErrRejected[] = "AUTH_RADIUS_REJECTED";
constexpr char kErrTestDirectiveDenied[] = "AUTH_RADIUS_TEST_DIRECTIVE_DENIED";
constexpr char kErrNoContinuation[] = "AUTH_RADIUS_NO_CONTINUE";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};

struct InstanceState {
    bool configured = false;
    scratchbird::security::plugins::radius::RadiusPluginConfig config;
    std::string config_error;
};

struct RadiusAuthRequest {
    std::string password;
    std::string authenticator;
    std::vector<std::string> directives;
};

struct RadiusProviderResult {
    sb_auth_rc_t rc = SB_AUTH_RC_DENY;
    uint32_t plugin_error_numeric = 16008;
    const char* plugin_error_code = kErrRejected;
};

std::mutex g_instances_mutex;
std::unordered_map<sb_auth_plugin_instance_t, InstanceState> g_instances;
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

inline std::string toString(sb_auth_slice_t slice) {
    if (!slice.ptr || slice.len == 0) {
        return "";
    }
    const char* begin = reinterpret_cast<const char*>(slice.ptr);
    return std::string(begin, begin + slice.len);
}

inline std::string trimAscii(std::string_view value) {
    std::size_t begin = 0;
    while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t' ||
                                    value[begin] == '\n' || value[begin] == '\r')) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t' ||
                           value[end - 1] == '\n' || value[end - 1] == '\r')) {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

inline bool parsePolicyBool(std::string_view text, bool* out_value) {
    if (!out_value) {
        return false;
    }
    if (text == "1" || text == "true" || text == "TRUE" || text == "yes" || text == "on") {
        *out_value = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "FALSE" || text == "no" || text == "off") {
        *out_value = false;
        return true;
    }
    return false;
}

inline bool isHexLower(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

inline bool isPasswordChar(char ch) {
    const unsigned char c = static_cast<unsigned char>(ch);
    return c >= 0x21u && c <= 0x7Eu;
}

inline bool parsePasswordField(std::string_view value, std::string* out_password) {
    if (!out_password || value.empty() || value.size() > kMaxPasswordBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isPasswordChar(ch)) {
            return false;
        }
    }
    out_password->assign(value.begin(), value.end());
    return true;
}

inline bool parseAuthenticatorField(std::string_view value, std::string* out_authenticator) {
    if (!out_authenticator || value.size() != kExpectedAuthenticatorHexBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isHexLower(ch)) {
            return false;
        }
    }
    out_authenticator->assign(value.begin(), value.end());
    return true;
}

inline bool isDirectiveTargetChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '.' || ch == '-' || ch == '_' || ch == ':';
}

inline bool parseSimulateDirectives(std::string_view value,
                                    std::vector<std::string>* out_directives) {
    if (!out_directives || value.empty()) {
        return false;
    }

    std::vector<std::string> parsed;
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        const std::size_t sep = value.find(',', cursor);
        const std::size_t end = (sep == std::string_view::npos) ? value.size() : sep;
        if (end <= cursor) {
            return false;
        }

        std::string directive = trimAscii(value.substr(cursor, end - cursor));
        if (directive.empty() || directive.size() > kMaxDirectiveBytes) {
            return false;
        }

        const std::size_t colon = directive.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= directive.size()) {
            return false;
        }

        const std::string_view action(directive.data(), colon);
        const std::string_view target(directive.data() + colon + 1,
                                      directive.size() - colon - 1);
        if (action != "timeout" && action != "reject") {
            return false;
        }
        if (target != "*") {
            if (target.size() > kMaxDirectiveTargetBytes) {
                return false;
            }
            for (char ch : target) {
                if (!isDirectiveTargetChar(ch)) {
                    return false;
                }
            }
        }

        if (std::find(parsed.begin(), parsed.end(), directive) != parsed.end()) {
            return false;
        }
        parsed.push_back(std::move(directive));
        if (parsed.size() > kMaxDirectiveCount) {
            return false;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (parsed.empty()) {
        return false;
    }
    *out_directives = std::move(parsed);
    return true;
}

inline bool parseAuthRequest(sb_auth_slice_t payload, RadiusAuthRequest* out_request) {
    if (!out_request || !payload.ptr || payload.len == 0 || payload.len > kMaxPayloadBytes) {
        return false;
    }

    const std::string payload_text = toString(payload);
    if (payload_text.empty()) {
        return false;
    }

    RadiusAuthRequest parsed{};
    bool password_seen = false;
    bool authenticator_seen = false;
    bool simulate_seen = false;

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
        if (key == "password" || key == "pwd") {
            if (password_seen || !parsePasswordField(value, &parsed.password)) {
                return false;
            }
            password_seen = true;
        } else if (key == "auth" || key == "authenticator" || key == "mac") {
            if (authenticator_seen ||
                !parseAuthenticatorField(value, &parsed.authenticator)) {
                return false;
            }
            authenticator_seen = true;
        } else if (key == "simulate") {
            if (simulate_seen || !parseSimulateDirectives(value, &parsed.directives)) {
                return false;
            }
            simulate_seen = true;
        } else {
            return false;
        }

        if (sep == std::string::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (!password_seen || !authenticator_seen) {
        return false;
    }

    *out_request = std::move(parsed);
    return true;
}

inline bool loadPolicyBool(const char* key, bool fallback) {
    if (!key || !g_host_api || !g_host_api->read_policy_value) {
        return fallback;
    }
    const sb_auth_slice_t key_slice{
        reinterpret_cast<const uint8_t*>(key),
        static_cast<uint32_t>(std::strlen(key))
    };
    sb_auth_slice_t value{};
    if (g_host_api->read_policy_value(key_slice, &value) != SB_AUTH_RC_OK || !value.ptr ||
        value.len == 0) {
        return fallback;
    }
    bool parsed = fallback;
    if (!parsePolicyBool(toString(value), &parsed)) {
        return fallback;
    }
    return parsed;
}

inline bool requestUsesTestDirectives(const RadiusAuthRequest& request) {
    return !request.directives.empty() || request.password == "__timeout__" ||
           request.password == "__reject__";
}

inline bool isTestRuntimeProfile(
    scratchbird::security::plugins::radius::RadiusRuntimeProfile profile) {
    return profile == scratchbird::security::plugins::radius::RadiusRuntimeProfile::TEST;
}

inline const char* runtimeProfileName(
    scratchbird::security::plugins::radius::RadiusRuntimeProfile profile) {
    return isTestRuntimeProfile(profile) ? "test" : "production";
}

inline bool hasDirective(const std::vector<std::string>& directives,
                         std::string_view action,
                         std::string_view endpoint) {
    for (const auto& directive : directives) {
        const std::size_t colon = directive.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= directive.size()) {
            continue;
        }
        const std::string_view d_action(directive.data(), colon);
        const std::string_view d_target(directive.data() + colon + 1,
                                        directive.size() - colon - 1);
        if (d_action == action && (d_target == endpoint || d_target == "*")) {
            return true;
        }
    }
    return false;
}

inline std::string computeRequestAuthenticator(const std::string& username,
                                               const std::string& password,
                                               const std::string& shared_secret) {
    const std::string message = username + "|" + password;
    std::string authenticator;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(shared_secret,
                                                                message,
                                                                &authenticator)) {
        return "";
    }
    return authenticator;
}

inline bool authenticatorsEqual(std::string_view lhs, std::string_view rhs) {
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

inline RadiusProviderResult evaluateRadiusProviderDecision(
    const InstanceState& state,
    const sb_auth_connection_ctx_v1* conn,
    const RadiusAuthRequest& request) {
    RadiusProviderResult result{};

    if (requestUsesTestDirectives(request)) {
        const bool allow_test_directives =
            isTestRuntimeProfile(state.config.runtime_profile) &&
            loadPolicyBool(kPolicyAllowTestDirectives, false);
        if (!allow_test_directives) {
            result.rc = SB_AUTH_RC_POLICY_VIOLATION;
            result.plugin_error_numeric = 16012;
            result.plugin_error_code = kErrTestDirectiveDenied;
            return result;
        }
    }

    if (state.config.request_timeout_ms == 0) {
        result.plugin_error_numeric = 16005;
        result.plugin_error_code = kErrTimeout;
        return result;
    }

    if (state.config.shared_secret_ref.empty()) {
        result.plugin_error_numeric = 16006;
        result.plugin_error_code = kErrSecretInvalid;
        return result;
    }

    scratchbird::security::plugins::secrets::SecretMaterial shared_secret{};
    if (scratchbird::security::plugins::secrets::resolveSecretReference(
            g_host_api,
            state.config.shared_secret_ref,
            &shared_secret) != scratchbird::security::plugins::secrets::SecretResolveStatus::OK) {
        result.plugin_error_numeric = 16006;
        result.plugin_error_code = kErrSecretInvalid;
        return result;
    }

    const std::string username =
        (conn->username.ptr && conn->username.len > 0) ? toString(conn->username) : "";
    const std::string expected_authenticator =
        computeRequestAuthenticator(username, request.password, shared_secret.value);
    if (request.authenticator.empty() ||
        !authenticatorsEqual(request.authenticator, expected_authenticator)) {
        result.plugin_error_numeric = 16006;
        result.plugin_error_code = kErrSecretInvalid;
        return result;
    }

    std::vector<std::string> candidate_servers;
    candidate_servers.reserve(state.config.radius_servers.size());
    for (const auto& server : state.config.radius_servers) {
        if (state.config.allowed_radius_endpoints.empty() ||
            std::find(state.config.allowed_radius_endpoints.begin(),
                      state.config.allowed_radius_endpoints.end(),
                      server) != state.config.allowed_radius_endpoints.end()) {
            candidate_servers.push_back(server);
        }
    }

    if (candidate_servers.empty()) {
        result.plugin_error_numeric = 16007;
        result.plugin_error_code = kErrPolicyDenied;
        return result;
    }

    bool saw_timeout = false;
    for (const auto& server : candidate_servers) {
        const bool timed_out = request.password == "__timeout__" ||
                               hasDirective(request.directives, "timeout", server);
        if (timed_out) {
            saw_timeout = true;
            continue;
        }

        const bool rejected = request.password == "__reject__" ||
                              hasDirective(request.directives, "reject", server);
        if (rejected) {
            result.plugin_error_numeric = 16008;
            result.plugin_error_code = kErrRejected;
            return result;
        }

        result.rc = SB_AUTH_RC_OK;
        result.plugin_error_numeric = 0;
        result.plugin_error_code = nullptr;
        return result;
    }

    if (saw_timeout) {
        result.plugin_error_numeric = 16005;
        result.plugin_error_code = kErrTimeout;
        return result;
    }

    result.plugin_error_numeric = 16008;
    result.plugin_error_code = kErrRejected;
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

    scratchbird::security::plugins::radius::RadiusPluginConfig parsed;
    std::string config_error;
    const auto status = scratchbird::security::plugins::radius::loadRadiusPluginConfig(
        values,
        parsed,
        &config_error);

    std::lock_guard<std::mutex> lock(g_instances_mutex);
    auto it = g_instances.find(instance);
    if (it == g_instances.end()) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }

    if (status != scratchbird::security::plugins::radius::RadiusPluginConfigStatus::OK) {
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
            "auth_plugin.radius.begin",
            kPluginId,
            kMethodRadiusPap,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result || !conn) {
        fillDeniedResult(out_result, 16001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }
    *inout_exchange = 0;

    if (!sliceEqualsLiteral(method_id, kMethodRadiusPap)) {
        fillDeniedResult(out_result, 16002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 16003, kErrMissingPayload);
        return finish(SB_AUTH_RC_DENY, kErrMissingPayload);
    }

    RadiusAuthRequest request{};
    if (!parseAuthRequest(client_payload, &request)) {
        fillDeniedResult(out_result, 16010, kErrMalformedPayload);
        return finish(SB_AUTH_RC_DENY, kErrMalformedPayload);
    }

    InstanceState state;
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        auto it = g_instances.find(instance);
        if (it == g_instances.end() || !it->second.configured) {
            fillDeniedResult(out_result, 16004, kErrConfigInvalid);
            return finish(SB_AUTH_RC_DENY, kErrConfigInvalid);
        }
        state = it->second;
    }

    const RadiusProviderResult provider_result =
        evaluateRadiusProviderDecision(state, conn, request);
    if (provider_result.rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result,
                         provider_result.plugin_error_numeric,
                         provider_result.plugin_error_code);
        return finish(provider_result.rc, provider_result.plugin_error_code);
    }

    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 35;
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
    if (conn->username.ptr && conn->username.len > 0) {
        out_result->principal.resolved_username = conn->username;
        if (g_host_api && g_host_api->resolve_user_by_name) {
            (void) g_host_api->resolve_user_by_name(conn->username,
                                                    out_result->principal.principal_uuid);
        }
    }
    return finish(SB_AUTH_RC_OK, nullptr);
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t /*exchange*/,
                                 sb_auth_slice_t /*client_payload*/,
                                 sb_auth_step_result_v1* out_result) {
    fillDeniedResult(out_result, 16009, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.radius.continue",
        kPluginId,
        kMethodRadiusPap,
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
    scratchbird::security::plugins::radius::RadiusRuntimeProfile runtime_profile =
        scratchbird::security::plugins::radius::RadiusRuntimeProfile::PRODUCTION;
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        auto it = g_instances.find(instance);
        configured = (it != g_instances.end() && it->second.configured);
        if (it != g_instances.end()) {
            runtime_profile = it->second.config.runtime_profile;
        }
    }

    g_health_payload = "{\"status\":\"ok\",\"plugin\":\"radius\",\"configured\":" +
                       std::string(configured ? "true" : "false") +
                       ",\"runtime_profile\":\"" + runtimeProfileName(runtime_profile) + "\"" +
                       ",\"provider_driver\":\"" +
                       std::string(kRadiusProviderDriverNative) + "\"" +
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
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodRadiusPap);
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
