/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "scratchbird/security/auth_plugin_abi_v1.h"

#include <dlfcn.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.pam";
constexpr char kMethodPamConversation[] = "scratchbird.auth.pam_conversation";
constexpr char kPolicyAllowTestDirectives[] = "auth.pam.allow_test_directives";

constexpr char kErrServiceNotAllowed[] = "AUTH_PAM_SERVICE_NOT_ALLOWED";
constexpr char kErrTimeout[] = "AUTH_PAM_CONVERSATION_TIMEOUT";
constexpr char kErrTestDirectiveDenied[] = "AUTH_PAM_TEST_DIRECTIVE_DENIED";
constexpr char kErrMalformedPayload[] = "AUTH_PAM_PAYLOAD_MALFORMED";
constexpr char kHealthDriverFragment[] = "\"provider_driver\":\"pam_native\"";
constexpr char kHealthRuntimeProfileFragment[] = "\"runtime_profile\":\"production\"";

uint64_t g_audit_events = 0;
std::unordered_map<std::string, std::string> g_policy_store{};

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

sb_auth_slice_t toSlice(const std::string& value) {
    return sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(value.data()),
        static_cast<uint32_t>(value.size())
    };
}

std::string toSoPath(const std::string& plugin_root) {
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_pam.so";
}

sb_auth_rc_t emitAuditEvent(sb_auth_slice_t /*event_name*/, sb_auth_slice_t /*event_json*/) {
    ++g_audit_events;
    return SB_AUTH_RC_OK;
}

sb_auth_rc_t resolveUserByName(sb_auth_slice_t username,
                               uint8_t out_user_uuid[SB_AUTH_UUID_BYTES]) {
    if (!username.ptr || username.len == 0 || !out_user_uuid) {
        return SB_AUTH_RC_DENY;
    }
    std::memset(out_user_uuid, 0, SB_AUTH_UUID_BYTES);
    out_user_uuid[0] = 0x93;
    return SB_AUTH_RC_OK;
}

sb_auth_rc_t readPolicyValue(sb_auth_slice_t key, sb_auth_slice_t* out_value) {
    if (!key.ptr || key.len == 0 || !out_value) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    const std::string key_text(reinterpret_cast<const char*>(key.ptr),
                               reinterpret_cast<const char*>(key.ptr) + key.len);
    const auto it = g_policy_store.find(key_text);
    if (it == g_policy_store.end()) {
        return SB_AUTH_RC_DENY;
    }
    out_value->ptr = reinterpret_cast<const uint8_t*>(it->second.data());
    out_value->len = static_cast<uint32_t>(it->second.size());
    return SB_AUTH_RC_OK;
}

sb_auth_rc_t begin(const sb_auth_plugin_api_v1* api,
                   sb_auth_plugin_instance_t instance,
                   sb_auth_connection_ctx_v1* conn,
                   const std::string& payload,
                   sb_auth_step_result_v1* out_result) {
    sb_auth_exchange_t exchange = 0;
    return api->begin_auth(instance,
                           toSlice(std::string(kMethodPamConversation)),
                           conn,
                           toSlice(payload),
                           &exchange,
                           out_result);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: pam_plugin_selftest <auth_plugin_output_root>\n";
        return 2;
    }

    const std::string path = toSoPath(argv[1]);
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    const char* dl_error = dlerror();
    if (!expect(handle != nullptr,
                "dlopen failed: " + std::string(dl_error ? dl_error : "unknown"))) {
        return 1;
    }

    auto* get_api = reinterpret_cast<sb_auth_plugin_get_api_v1_fn>(
        dlsym(handle, "sb_auth_plugin_get_api_v1"));
    if (!expect(get_api != nullptr, "missing sb_auth_plugin_get_api_v1")) {
        dlclose(handle);
        return 1;
    }

    sb_auth_host_api_v1 host_api{};
    host_api.struct_size = sizeof(sb_auth_host_api_v1);
    host_api.emit_audit_event = &emitAuditEvent;
    host_api.resolve_user_by_name = &resolveUserByName;
    host_api.read_policy_value = &readPolicyValue;

    const sb_auth_plugin_descriptor_v1* descriptor = nullptr;
    const sb_auth_plugin_api_v1* api = nullptr;
    bool ok = expect(get_api(SB_AUTH_ABI_MAJOR, &host_api, &descriptor, &api) == SB_AUTH_RC_OK,
                     "get_api failed");
    ok = expect(descriptor != nullptr && api != nullptr, "null descriptor/api") && ok;
    if (!ok) {
        dlclose(handle);
        return 1;
    }

    sb_auth_plugin_instance_t instance = 0;
    ok = expect(api->create_instance(&instance) == SB_AUTH_RC_OK, "create_instance failed") && ok;

    const std::string config_json =
        "{\"service_name\":\"login\",\"allowed_modules\":[\"pam_unix.so\",\"pam_sss.so\"],\"conversation_timeout_ms\":1000}";
    ok = expect(api->configure_instance(instance, toSlice(config_json)) == SB_AUTH_RC_OK,
                "configure_instance failed") &&
         ok;

    sb_auth_slice_t health_json{};
    sb_auth_rc_t rc = api->health_check(instance, &health_json);
    ok = expect(rc == SB_AUTH_RC_OK && health_json.ptr != nullptr && health_json.len > 0,
                "health_check failed") &&
         ok;
    const std::string health_text(
        reinterpret_cast<const char*>(health_json.ptr),
        reinterpret_cast<const char*>(health_json.ptr) + health_json.len);
    ok = expect(health_text.find(kHealthDriverFragment) != std::string::npos,
                "pam health provider_driver missing") &&
         ok;
    ok = expect(health_text.find(kHealthRuntimeProfileFragment) != std::string::npos,
                "pam health runtime_profile missing") &&
         ok;

    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_LOCAL;
    std::string username = "pam_user";
    conn.username = toSlice(username);

    sb_auth_step_result_v1 result{};

    // Hardening H1: test directives denied by default unless policy enables them.
    const std::string timeout_payload =
        "service=login;module=pam_unix.so;password=__timeout__;prompt=hidden";
    rc = begin(api, instance, &conn, timeout_payload, &result);
    ok = expect(rc == SB_AUTH_RC_POLICY_VIOLATION,
                "synthetic timeout directive should deny when policy disabled") &&
         ok;
    ok = expect(std::string(result.plugin_error_code) == kErrTestDirectiveDenied,
                "test-directive deny error code mismatch") &&
         ok;

    g_policy_store[kPolicyAllowTestDirectives] = "true";
    std::memset(&result, 0, sizeof(result));
    rc = begin(api, instance, &conn, timeout_payload, &result);
    ok = expect(rc == SB_AUTH_RC_POLICY_VIOLATION,
                "synthetic timeout directive should remain denied under production profile") &&
         ok;
    ok = expect(std::string(result.plugin_error_code) == kErrTestDirectiveDenied,
                "production profile directive deny error code mismatch") &&
         ok;

    // In test profile with explicit policy toggle, synthetic directives are allowed.
    sb_auth_plugin_instance_t test_profile_instance = 0;
    ok = expect(api->create_instance(&test_profile_instance) == SB_AUTH_RC_OK,
                "create_instance for test profile failed") &&
         ok;
    const std::string config_json_test_profile =
        "{\"service_name\":\"login\",\"allowed_modules\":[\"pam_unix.so\",\"pam_sss.so\"],\"conversation_timeout_ms\":1000,\"runtime_profile\":\"test\"}";
    ok = expect(api->configure_instance(test_profile_instance, toSlice(config_json_test_profile)) ==
                    SB_AUTH_RC_OK,
                "configure_instance for test profile failed") &&
         ok;

    std::memset(&result, 0, sizeof(result));
    rc = begin(api, test_profile_instance, &conn, timeout_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY,
                "synthetic timeout directive should map to timeout in test profile") &&
         ok;
    ok = expect(std::string(result.plugin_error_code) == kErrTimeout,
                "test profile timeout error code mismatch") &&
         ok;

    // AT-B12-01: allowed service authenticates.
    const std::string allowed_payload =
        "service=login;module=pam_unix.so;password=goodpass;prompt=hidden";
    std::memset(&result, 0, sizeof(result));
    rc = begin(api, instance, &conn, allowed_payload, &result);
    ok = expect(rc == SB_AUTH_RC_OK, "allowed service should authenticate") && ok;

    // AT-B12-02: blocked service denied with policy code.
    std::memset(&result, 0, sizeof(result));
    const std::string blocked_payload =
        "service=sshd;module=pam_unix.so;password=goodpass;prompt=hidden";
    rc = begin(api, instance, &conn, blocked_payload, &result);
    ok = expect(rc == SB_AUTH_RC_POLICY_VIOLATION,
                "blocked service should deny with policy violation") &&
         ok;
    ok = expect(std::string(result.plugin_error_code) == kErrServiceNotAllowed,
                "blocked service error code mismatch") &&
         ok;

    // AT-B12-03: unknown payload key denied (strict schema).
    std::memset(&result, 0, sizeof(result));
    const std::string unknown_key_payload =
        "service=login;module=pam_unix.so;password=goodpass;prompt=hidden;extra=1";
    rc = begin(api, instance, &conn, unknown_key_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "unknown payload key should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "unknown payload key error code mismatch") &&
         ok;

    // AT-B12-04: duplicate password aliases denied (no overwrite semantics).
    std::memset(&result, 0, sizeof(result));
    const std::string duplicate_password_payload =
        "service=login;module=pam_unix.so;password=goodpass;pwd=goodpass;prompt=hidden";
    rc = begin(api, instance, &conn, duplicate_password_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "duplicate password aliases should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "duplicate password aliases error code mismatch") &&
         ok;

    // AT-B12-05: raw payload denied (must be key-value schema).
    std::memset(&result, 0, sizeof(result));
    rc = begin(api, instance, &conn, "goodpass", &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "raw payload should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "raw payload error code mismatch") &&
         ok;

    ok = expect(g_audit_events >= 7, "expected audit events for pam begin calls") && ok;

    api->destroy_instance(test_profile_instance);
    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "pam_plugin_selftest: PASS\n";
    return 0;
}
