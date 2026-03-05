/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "pam_plugin_config.h"
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

namespace {

constexpr char kPluginId[] = "scratchbird.auth.pam";
constexpr char kPluginVersion[] = "2.1.0";
constexpr char kMethodPamConversation[] = "scratchbird.auth.pam_conversation";
constexpr char kPolicyAllowTestDirectives[] = "auth.pam.allow_test_directives";
constexpr char kPamProviderDriverNative[] = "pam_native";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxPayloadBytes = 2048;
constexpr uint32_t kMaxServiceBytes = 64;
constexpr uint32_t kMaxModuleBytes = 128;
constexpr uint32_t kMaxPasswordBytes = 256;

constexpr char kErrBadRequest[] = "AUTH_PAM_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_PAM_METHOD_UNKNOWN";
constexpr char kErrMissingPayload[] = "AUTH_PAM_PAYLOAD_MISSING";
constexpr char kErrMalformedPayload[] = "AUTH_PAM_PAYLOAD_MALFORMED";
constexpr char kErrConfigInvalid[] = "AUTH_PAM_CONFIG_INVALID";
constexpr char kErrTimeout[] = "AUTH_PAM_CONVERSATION_TIMEOUT";
constexpr char kErrServiceNotAllowed[] = "AUTH_PAM_SERVICE_NOT_ALLOWED";
constexpr char kErrModuleNotAllowed[] = "AUTH_PAM_MODULE_NOT_ALLOWED";
constexpr char kErrInsecurePrompt[] = "AUTH_PAM_INSECURE_PROMPT";
constexpr char kErrDenied[] = "AUTH_PAM_DENIED";
constexpr char kErrTestDirectiveDenied[] = "AUTH_PAM_TEST_DIRECTIVE_DENIED";
constexpr char kErrNoContinuation[] = "AUTH_PAM_NO_CONTINUE";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};

struct InstanceState {
    bool configured = false;
    scratchbird::security::plugins::pam::PamPluginConfig config;
    std::string config_error;
};

struct PamConversationRequest {
    std::string service;
    std::string module;
    std::string password;
    std::string prompt_mode;
};

struct PamProviderResult {
    sb_auth_rc_t rc = SB_AUTH_RC_DENY;
    uint32_t plugin_error_numeric = 17009;
    const char* plugin_error_code = kErrDenied;
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

inline bool isServiceChar(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '_' || ch == '-' || ch == '.';
}

inline bool isModuleChar(char ch) {
    if (isServiceChar(ch)) {
        return true;
    }
    return ch == '/';
}

inline bool parseServiceField(std::string_view value, std::string* out_service) {
    if (!out_service || value.empty() || value.size() > kMaxServiceBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isServiceChar(ch)) {
            return false;
        }
    }
    out_service->assign(value.begin(), value.end());
    return true;
}

inline bool parseModuleField(std::string_view value, std::string* out_module) {
    if (!out_module || value.empty() || value.size() > kMaxModuleBytes) {
        return false;
    }
    for (char ch : value) {
        if (!isModuleChar(ch)) {
            return false;
        }
    }
    out_module->assign(value.begin(), value.end());
    return true;
}

inline bool parsePasswordField(std::string_view value, std::string* out_password) {
    if (!out_password || value.empty() || value.size() > kMaxPasswordBytes) {
        return false;
    }
    for (char ch : value) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x21u || c > 0x7Eu || ch == ';' || ch == '=') {
            return false;
        }
    }
    out_password->assign(value.begin(), value.end());
    return true;
}

inline bool parsePromptModeField(std::string_view value, std::string* out_prompt_mode) {
    if (!out_prompt_mode) {
        return false;
    }
    if (value == "hidden" || value == "secret") {
        out_prompt_mode->assign(value.begin(), value.end());
        return true;
    }
    return false;
}

inline bool parseConversationRequest(sb_auth_slice_t payload,
                                     PamConversationRequest* out_request) {
    if (!out_request || !payloadLooksSafe(payload)) {
        return false;
    }

    const std::string payload_text = toString(payload);
    if (payload_text.empty()) {
        return false;
    }

    PamConversationRequest parsed{};
    bool service_seen = false;
    bool module_seen = false;
    bool password_seen = false;
    bool prompt_seen = false;

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

        if (key == "service") {
            if (service_seen || !parseServiceField(value, &parsed.service)) {
                return false;
            }
            service_seen = true;
        } else if (key == "module") {
            if (module_seen || !parseModuleField(value, &parsed.module)) {
                return false;
            }
            module_seen = true;
        } else if (key == "password" || key == "pwd") {
            if (password_seen || !parsePasswordField(value, &parsed.password)) {
                return false;
            }
            password_seen = true;
        } else if (key == "prompt") {
            if (prompt_seen || !parsePromptModeField(value, &parsed.prompt_mode)) {
                return false;
            }
            prompt_seen = true;
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

inline bool isSyntheticPasswordDirective(const std::string& password) {
    return password == "__timeout__" || password == "__deny__";
}

inline bool isTestRuntimeProfile(
    scratchbird::security::plugins::pam::PamRuntimeProfile profile) {
    return profile == scratchbird::security::plugins::pam::PamRuntimeProfile::TEST;
}

inline const char* runtimeProfileName(
    scratchbird::security::plugins::pam::PamRuntimeProfile profile) {
    return isTestRuntimeProfile(profile) ? "test" : "production";
}

inline PamProviderResult evaluatePamProviderDecision(const InstanceState& state,
                                                     const PamConversationRequest& request) {
    PamProviderResult result{};

    if (isSyntheticPasswordDirective(request.password)) {
        const bool allow_test_directives =
            isTestRuntimeProfile(state.config.runtime_profile) &&
            loadPolicyBool(kPolicyAllowTestDirectives, false);
        if (!allow_test_directives) {
            result.rc = SB_AUTH_RC_POLICY_VIOLATION;
            result.plugin_error_numeric = 17012;
            result.plugin_error_code = kErrTestDirectiveDenied;
            return result;
        }
    }

    if (state.config.conversation_timeout_ms == 0 || request.password == "__timeout__") {
        result.plugin_error_numeric = 17005;
        result.plugin_error_code = kErrTimeout;
        return result;
    }

    const std::string effective_service =
        request.service.empty() ? state.config.service_name : request.service;
    const std::string effective_module =
        request.module.empty() ?
            (state.config.allowed_modules.empty() ? "" : state.config.allowed_modules.front()) :
            request.module;
    const std::string prompt_mode = request.prompt_mode.empty() ? "hidden" : request.prompt_mode;

    if (effective_service.empty() || effective_service != state.config.service_name) {
        result.rc = SB_AUTH_RC_POLICY_VIOLATION;
        result.plugin_error_numeric = 17006;
        result.plugin_error_code = kErrServiceNotAllowed;
        return result;
    }

    if (!state.config.allowed_modules.empty() &&
        std::find(state.config.allowed_modules.begin(),
                  state.config.allowed_modules.end(),
                  effective_module) == state.config.allowed_modules.end()) {
        result.rc = SB_AUTH_RC_POLICY_VIOLATION;
        result.plugin_error_numeric = 17007;
        result.plugin_error_code = kErrModuleNotAllowed;
        return result;
    }

    if (prompt_mode != "hidden" && prompt_mode != "secret") {
        result.rc = SB_AUTH_RC_POLICY_VIOLATION;
        result.plugin_error_numeric = 17008;
        result.plugin_error_code = kErrInsecurePrompt;
        return result;
    }

    if (request.password == "__deny__") {
        result.plugin_error_numeric = 17009;
        result.plugin_error_code = kErrDenied;
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

    scratchbird::security::plugins::pam::PamPluginConfig parsed;
    std::string config_error;
    const auto status = scratchbird::security::plugins::pam::loadPamPluginConfig(
        values,
        parsed,
        &config_error);

    std::lock_guard<std::mutex> lock(g_instances_mutex);
    auto it = g_instances.find(instance);
    if (it == g_instances.end()) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }

    if (status != scratchbird::security::plugins::pam::PamPluginConfigStatus::OK) {
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
            "auth_plugin.pam.begin",
            kPluginId,
            kMethodPamConversation,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result || !conn) {
        fillDeniedResult(out_result, 17001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }
    *inout_exchange = 0;

    if (!sliceEqualsLiteral(method_id, kMethodPamConversation)) {
        fillDeniedResult(out_result, 17002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 17003, kErrMissingPayload);
        return finish(SB_AUTH_RC_DENY, kErrMissingPayload);
    }

    PamConversationRequest request{};
    if (!parseConversationRequest(client_payload, &request)) {
        fillDeniedResult(out_result, 17010, kErrMalformedPayload);
        return finish(SB_AUTH_RC_DENY, kErrMalformedPayload);
    }

    InstanceState state;
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        auto it = g_instances.find(instance);
        if (it == g_instances.end() || !it->second.configured) {
            fillDeniedResult(out_result, 17004, kErrConfigInvalid);
            return finish(SB_AUTH_RC_DENY, kErrConfigInvalid);
        }
        state = it->second;
    }

    const PamProviderResult provider_result = evaluatePamProviderDecision(state, request);
    if (provider_result.rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result,
                         provider_result.plugin_error_numeric,
                         provider_result.plugin_error_code);
        return finish(provider_result.rc, provider_result.plugin_error_code);
    }

    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 30;
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
    fillDeniedResult(out_result, 17011, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.pam.continue",
        kPluginId,
        kMethodPamConversation,
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
    scratchbird::security::plugins::pam::PamRuntimeProfile runtime_profile =
        scratchbird::security::plugins::pam::PamRuntimeProfile::PRODUCTION;
    {
        std::lock_guard<std::mutex> lock(g_instances_mutex);
        auto it = g_instances.find(instance);
        configured = (it != g_instances.end() && it->second.configured);
        if (it != g_instances.end()) {
            runtime_profile = it->second.config.runtime_profile;
        }
    }

    g_health_payload = "{\"status\":\"ok\",\"plugin\":\"pam\",\"configured\":" +
                       std::string(configured ? "true" : "false") +
                       ",\"runtime_profile\":\"" + runtimeProfileName(runtime_profile) + "\"" +
                       ",\"provider_driver\":\"" + std::string(kPamProviderDriverNative) + "\"" +
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
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodPamConversation);
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
