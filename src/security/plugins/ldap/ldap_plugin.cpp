/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "ldap_plugin_config.h"
#include "../auth_plugin_observability.h"
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

constexpr char kPluginId[] = "scratchbird.auth.ldap";
constexpr char kPluginVersion[] = "2.1.0";
constexpr char kMethodLdapBind[] = "scratchbird.auth.ldap_bind";
constexpr char kPolicyAllowTestDirectives[] = "auth.ldap.allow_test_directives";
constexpr char kLdapProviderDriverNative[] = "ldap_native";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxPayloadBytes = 4096;
constexpr uint32_t kMaxUsernameBytes = 128;
constexpr uint32_t kMaxPasswordBytes = 256;
constexpr uint32_t kMaxEndpointBytes = 128;
constexpr uint32_t kMaxGroupBytes = 128;
constexpr uint32_t kMaxGroups = 32;

constexpr char kErrBadRequest[] = "AUTH_LDAP_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_LDAP_METHOD_UNKNOWN";
constexpr char kErrMissingPayload[] = "AUTH_LDAP_PAYLOAD_MISSING";
constexpr char kErrMalformedPayload[] = "AUTH_LDAP_PAYLOAD_MALFORMED";
constexpr char kErrConfigInvalid[] = "AUTH_LDAP_CONFIG_INVALID";
constexpr char kErrStartTlsRequired[] = "AUTH_LDAP_STARTTLS_REQUIRED";
constexpr char kErrEndpointDenied[] = "AUTH_LDAP_ENDPOINT_NOT_ALLOWED";
constexpr char kErrBindFailed[] = "AUTH_LDAP_BIND_FAILED";
constexpr char kErrRoleMappingMissing[] = "AUTH_LDAP_ROLE_MAPPING_MISSING";
constexpr char kErrTimeout[] = "AUTH_LDAP_TIMEOUT";
constexpr char kErrTestDirectiveDenied[] = "AUTH_LDAP_TEST_DIRECTIVE_DENIED";
constexpr char kErrNoContinuation[] = "AUTH_LDAP_NO_CONTINUE";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};

struct InstanceState {
    bool configured = false;
    scratchbird::security::plugins::ldap::LdapPluginConfig config;
    std::string config_error;
};

struct LdapBindRequest {
    std::string username;
    std::string password;
    std::string endpoint;
    bool starttls_enabled = false;
    bool starttls_seen = false;
    std::vector<std::string> groups;
};

struct LdapProviderResult {
    sb_auth_rc_t rc = SB_AUTH_RC_DENY;
    uint32_t plugin_error_numeric = 13008;
    const char* plugin_error_code = kErrBindFailed;
    std::string resolved_username;
    std::string mapped_role;
};

std::mutex g_instances_mutex;
std::unordered_map<sb_auth_plugin_instance_t, InstanceState> g_instances;
scratchbird::security::plugins::observability::OutcomeCounters g_counters{};
thread_local std::string g_health_payload;
thread_local std::string g_bound_username;

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

inline bool parseBool(std::string_view text, bool* out_value) {
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
    if (!parseBool(toString(value), &parsed)) {
        return fallback;
    }
    return parsed;
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
            c == static_cast<uint8_t>(',') || c == static_cast<uint8_t>('{') ||
            c == static_cast<uint8_t>('}') || c == static_cast<uint8_t>('/');
        if (!allowed) {
            return false;
        }
    }

    return true;
}

inline bool isUsernameChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '_' || ch == '-' || ch == '.';
}

inline bool isEndpointChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '_' || ch == '-' || ch == '.' || ch == ':';
}

inline bool isGroupChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '_' || ch == '-' || ch == '.' || ch == ':' ||
           ch == '=' || ch == '/';
}

inline bool parseUsernameField(std::string_view value, std::string* out_username) {
    if (!out_username || value.empty() || value.size() > kMaxUsernameBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isUsernameChar(ch)) {
            return false;
        }
    }
    out_username->assign(value.begin(), value.end());
    return true;
}

inline bool parsePasswordField(std::string_view value, std::string* out_password) {
    if (!out_password || value.empty() || value.size() > kMaxPasswordBytes) {
        return false;
    }
    for (char ch : value) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x21u || c > 0x7Eu || ch == ';') {
            return false;
        }
    }
    out_password->assign(value.begin(), value.end());
    return true;
}

inline bool parseEndpointField(std::string_view value, std::string* out_endpoint) {
    if (!out_endpoint || value.empty() || value.size() > kMaxEndpointBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isEndpointChar(ch)) {
            return false;
        }
    }
    out_endpoint->assign(value.begin(), value.end());
    return true;
}

inline bool parseGroupList(std::string_view value, std::vector<std::string>* out_groups) {
    if (!out_groups || value.empty()) {
        return false;
    }

    std::vector<std::string> groups;
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        const std::size_t sep = value.find(',', cursor);
        const std::size_t end = (sep == std::string_view::npos) ? value.size() : sep;
        if (end <= cursor) {
            return false;
        }

        const std::string_view token = value.substr(cursor, end - cursor);
        if (token.empty() || token.size() > kMaxGroupBytes) {
            return false;
        }
        for (char ch : token) {
            if (!isGroupChar(ch)) {
                return false;
            }
        }

        const std::string token_text(token.begin(), token.end());
        if (std::find(groups.begin(), groups.end(), token_text) != groups.end()) {
            return false;
        }
        groups.push_back(token_text);
        if (groups.size() > kMaxGroups) {
            return false;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (groups.empty()) {
        return false;
    }
    *out_groups = std::move(groups);
    return true;
}

inline std::vector<std::string> splitList(std::string_view value, char delimiter) {
    std::vector<std::string> out;
    std::size_t cursor = 0;
    while (cursor <= value.size()) {
        const std::size_t sep = value.find(delimiter, cursor);
        const std::size_t end = (sep == std::string_view::npos) ? value.size() : sep;
        const std::string token = trimAscii(value.substr(cursor, end - cursor));
        if (!token.empty()) {
            out.push_back(token);
        }
        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }
    return out;
}

inline bool parseBindRequest(sb_auth_slice_t payload, LdapBindRequest* out_request) {
    if (!out_request || !payloadLooksSafe(payload)) {
        return false;
    }

    const std::string payload_text = toString(payload);
    if (payload_text.empty()) {
        return false;
    }

    LdapBindRequest parsed{};
    bool username_seen = false;
    bool password_seen = false;
    bool groups_seen = false;
    bool starttls_seen = false;
    bool endpoint_seen = false;

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

        if (key == "user" || key == "username") {
            if (username_seen || !parseUsernameField(value, &parsed.username)) {
                return false;
            }
            username_seen = true;
        } else if (key == "password" || key == "pwd") {
            if (password_seen || !parsePasswordField(value, &parsed.password)) {
                return false;
            }
            password_seen = true;
        } else if (key == "groups") {
            if (groups_seen || !parseGroupList(value, &parsed.groups)) {
                return false;
            }
            groups_seen = true;
        } else if (key == "starttls") {
            if (starttls_seen || !parseBool(value, &parsed.starttls_enabled)) {
                return false;
            }
            parsed.starttls_seen = true;
            starttls_seen = true;
        } else if (key == "endpoint") {
            if (endpoint_seen || !parseEndpointField(value, &parsed.endpoint)) {
                return false;
            }
            endpoint_seen = true;
        } else {
            return false;
        }

        if (sep == std::string::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (!password_seen) {
        return false;
    }

    *out_request = std::move(parsed);
    return true;
}

inline bool isSyntheticPasswordDirective(const std::string& password) {
    return password == "__timeout__" || password == "__deny__";
}

inline bool isTestRuntimeProfile(scratchbird::security::plugins::ldap::LdapRuntimeProfile profile) {
    return profile == scratchbird::security::plugins::ldap::LdapRuntimeProfile::TEST;
}

inline const char* runtimeProfileName(
    scratchbird::security::plugins::ldap::LdapRuntimeProfile profile) {
    return isTestRuntimeProfile(profile) ? "test" : "production";
}

inline bool startsWith(const std::string& value, const char* prefix) {
    if (!prefix) {
        return false;
    }
    const std::size_t n = std::strlen(prefix);
    return value.size() >= n && value.compare(0, n, prefix) == 0;
}

inline std::unordered_map<std::string, std::string> parseGroupRoleMap(const std::string& text) {
    std::unordered_map<std::string, std::string> out;
    for (const auto& entry : splitList(text, ',')) {
        const std::size_t colon = entry.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= entry.size()) {
            continue;
        }
        const std::string group = trimAscii(std::string_view(entry.data(), colon));
        const std::string role = trimAscii(
            std::string_view(entry.data() + colon + 1, entry.size() - colon - 1));
        if (!group.empty() && !role.empty()) {
            out[group] = role;
        }
    }
    return out;
}

inline bool findMappedRole(const std::vector<std::string>& groups,
                           const std::unordered_map<std::string, std::string>& role_map,
                           std::string* out_role) {
    if (!out_role) {
        return false;
    }
    for (const auto& group : groups) {
        auto it = role_map.find(group);
        if (it != role_map.end()) {
            *out_role = it->second;
            return true;
        }
    }
    return false;
}

inline LdapProviderResult evaluateLdapProviderDecision(const InstanceState& state,
                                                       const sb_auth_connection_ctx_v1* conn,
                                                       const LdapBindRequest& request) {
    LdapProviderResult result{};

    if (isSyntheticPasswordDirective(request.password)) {
        const bool allow_test_directives =
            isTestRuntimeProfile(state.config.runtime_profile) &&
            loadPolicyBool(kPolicyAllowTestDirectives, false);
        if (!allow_test_directives) {
            result.rc = SB_AUTH_RC_POLICY_VIOLATION;
            result.plugin_error_numeric = 13012;
            result.plugin_error_code = kErrTestDirectiveDenied;
            return result;
        }
    }

    if (state.config.connect_timeout_ms == 0 || request.password == "__timeout__") {
        result.plugin_error_numeric = 13011;
        result.plugin_error_code = kErrTimeout;
        return result;
    }

    const std::string endpoint = request.endpoint.empty() ? toString(conn->server_address)
                                                          : request.endpoint;
    if (!state.config.allowed_ldap_endpoints.empty() && !endpoint.empty()) {
        const auto allowed = std::find(state.config.allowed_ldap_endpoints.begin(),
                                       state.config.allowed_ldap_endpoints.end(),
                                       endpoint);
        if (allowed == state.config.allowed_ldap_endpoints.end()) {
            result.rc = SB_AUTH_RC_POLICY_VIOLATION;
            result.plugin_error_numeric = 13006;
            result.plugin_error_code = kErrEndpointDenied;
            return result;
        }
    }

    const bool using_ldaps = startsWith(state.config.ldap_uri, "ldaps://");
    if (state.config.require_starttls && !using_ldaps && !request.starttls_enabled) {
        result.plugin_error_numeric = 13005;
        result.plugin_error_code = kErrStartTlsRequired;
        return result;
    }

    if (request.password.empty() || request.password == "__deny__") {
        result.plugin_error_numeric = 13008;
        result.plugin_error_code = kErrBindFailed;
        return result;
    }

    const auto group_role_map = parseGroupRoleMap(state.config.group_role_map);
    if (group_role_map.empty() ||
        !findMappedRole(request.groups, group_role_map, &result.mapped_role)) {
        result.rc = SB_AUTH_RC_POLICY_VIOLATION;
        result.plugin_error_numeric = 13009;
        result.plugin_error_code = kErrRoleMappingMissing;
        return result;
    }

    result.resolved_username =
        !request.username.empty() ? request.username : toString(conn->username);
    if (result.resolved_username.empty()) {
        result.plugin_error_numeric = 13008;
        result.plugin_error_code = kErrBindFailed;
        return result;
    }

    result.rc = SB_AUTH_RC_OK;
    result.plugin_error_numeric = 0;
    result.plugin_error_code = nullptr;
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

    scratchbird::security::plugins::ldap::LdapPluginConfig parsed;
    std::string config_error;
    const auto status = scratchbird::security::plugins::ldap::loadLdapPluginConfig(
        values,
        parsed,
        &config_error);

    std::lock_guard<std::mutex> lock(g_instances_mutex);
    auto it = g_instances.find(instance);
    if (it == g_instances.end()) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }

    if (status != scratchbird::security::plugins::ldap::LdapPluginConfigStatus::OK) {
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
            "auth_plugin.ldap.begin",
            kPluginId,
            kMethodLdapBind,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result || !conn) {
        fillDeniedResult(out_result, 13001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }
    *inout_exchange = 0;

    if (!sliceEqualsLiteral(method_id, kMethodLdapBind)) {
        fillDeniedResult(out_result, 13002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 13003, kErrMissingPayload);
        return finish(SB_AUTH_RC_DENY, kErrMissingPayload);
    }

    LdapBindRequest request{};
    if (!parseBindRequest(client_payload, &request)) {
        fillDeniedResult(out_result, 13007, kErrMalformedPayload);
        return finish(SB_AUTH_RC_DENY, kErrMalformedPayload);
    }

    InstanceState state;
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        auto it = g_instances.find(instance);
        if (it == g_instances.end() || !it->second.configured) {
            fillDeniedResult(out_result, 13004, kErrConfigInvalid);
            return finish(SB_AUTH_RC_DENY, kErrConfigInvalid);
        }
        state = it->second;
    }

    const LdapProviderResult provider_result =
        evaluateLdapProviderDecision(state, conn, request);
    if (provider_result.rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result,
                         provider_result.plugin_error_numeric,
                         provider_result.plugin_error_code);
        return finish(provider_result.rc, provider_result.plugin_error_code);
    }

    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 50;
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");

    g_bound_username = provider_result.resolved_username;
    out_result->principal.resolved_username = sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(g_bound_username.data()),
        static_cast<uint32_t>(g_bound_username.size())
    };

    if (g_host_api && g_host_api->resolve_user_by_name) {
        const sb_auth_rc_t resolve_rc = g_host_api->resolve_user_by_name(
            out_result->principal.resolved_username,
            out_result->principal.principal_uuid);
        if (resolve_rc != SB_AUTH_RC_OK) {
            fillDeniedResult(out_result, 13008, kErrBindFailed);
            return finish(SB_AUTH_RC_DENY, kErrBindFailed);
        }
    }

    (void) provider_result.mapped_role;
    return finish(SB_AUTH_RC_OK, nullptr);
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t /*exchange*/,
                                 sb_auth_slice_t /*client_payload*/,
                                 sb_auth_step_result_v1* out_result) {
    fillDeniedResult(out_result, 13010, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.ldap.continue",
        kPluginId,
        kMethodLdapBind,
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
    scratchbird::security::plugins::ldap::LdapRuntimeProfile runtime_profile =
        scratchbird::security::plugins::ldap::LdapRuntimeProfile::PRODUCTION;
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        auto it = g_instances.find(instance);
        configured = (it != g_instances.end() && it->second.configured);
        if (it != g_instances.end()) {
            runtime_profile = it->second.config.runtime_profile;
        }
    }

    g_health_payload = "{\"status\":\"ok\",\"plugin\":\"ldap\",\"configured\":" +
                       std::string(configured ? "true" : "false") +
                       ",\"runtime_profile\":\"" + runtimeProfileName(runtime_profile) + "\"" +
                       ",\"provider_driver\":\"" + std::string(kLdapProviderDriverNative) + "\"" +
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
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodLdapBind);
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
