/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "scratchbird/security/auth_plugin_abi_v1.h"
#include "auth_plugin_crypto.h"

#include <dlfcn.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.radius";
constexpr char kMethodRadiusPap[] = "scratchbird.auth.radius_pap";

constexpr char kPolicySharedSecretRef[] = "auth.radius.shared_secret_ref";
constexpr char kPolicyAllowTestDirectives[] = "auth.radius.allow_test_directives";

constexpr char kErrSecretInvalid[] = "AUTH_RADIUS_SHARED_SECRET_INVALID";
constexpr char kErrRejected[] = "AUTH_RADIUS_REJECTED";
constexpr char kErrTestDirectiveDenied[] = "AUTH_RADIUS_TEST_DIRECTIVE_DENIED";
constexpr char kErrMalformedPayload[] = "AUTH_RADIUS_PAYLOAD_MALFORMED";
constexpr char kHealthDriverFragment[] = "\"provider_driver\":\"radius_native\"";
constexpr char kHealthRuntimeProfileFragment[] = "\"runtime_profile\":\"production\"";

std::unordered_map<std::string, std::string> g_policy_store{
    {kPolicySharedSecretRef, "radius-secret-v1"},
};
uint64_t g_audit_events = 0;

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
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_radius.so";
}

std::string computeAuthenticator(const std::string& username,
                                 const std::string& password,
                                 const std::string& shared_secret) {
    std::string authenticator;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(shared_secret,
                                                                username + "|" + password,
                                                                &authenticator)) {
        return "";
    }
    return authenticator;
}

sb_auth_rc_t readPolicyValue(sb_auth_slice_t key, sb_auth_slice_t* out_value) {
    if (!key.ptr || key.len == 0 || !out_value) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    const std::string key_text(reinterpret_cast<const char*>(key.ptr),
                               reinterpret_cast<const char*>(key.ptr) + key.len);
    auto it = g_policy_store.find(key_text);
    if (it == g_policy_store.end()) {
        return SB_AUTH_RC_DENY;
    }
    out_value->ptr = reinterpret_cast<const uint8_t*>(it->second.data());
    out_value->len = static_cast<uint32_t>(it->second.size());
    return SB_AUTH_RC_OK;
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
    out_user_uuid[0] = 0xA3;
    return SB_AUTH_RC_OK;
}

int constTimeEqual(const uint8_t* a, const uint8_t* b, uint32_t len) {
    if (!a || !b) {
        return 0;
    }
    uint8_t diff = 0;
    for (uint32_t i = 0; i < len; ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0 ? 1 : 0;
}

sb_auth_rc_t begin(const sb_auth_plugin_api_v1* api,
                   sb_auth_plugin_instance_t instance,
                   sb_auth_connection_ctx_v1* conn,
                   const std::string& payload,
                   sb_auth_step_result_v1* out_result) {
    sb_auth_exchange_t exchange = 0;
    return api->begin_auth(instance,
                           toSlice(std::string(kMethodRadiusPap)),
                           conn,
                           toSlice(payload),
                           &exchange,
                           out_result);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: radius_plugin_selftest <auth_plugin_output_root>\n";
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
    host_api.read_policy_value = &readPolicyValue;
    host_api.emit_audit_event = &emitAuditEvent;
    host_api.resolve_user_by_name = &resolveUserByName;
    host_api.const_time_equal = &constTimeEqual;

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
        "{\"radius_servers\":[\"radius-primary\",\"radius-secondary\"],\"shared_secret_ref\":\"auth.radius.shared_secret_ref\",\"request_timeout_ms\":1000,\"allowed_radius_endpoints\":[\"radius-primary\",\"radius-secondary\"]}";
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
                "radius health provider_driver missing") &&
         ok;
    ok = expect(health_text.find(kHealthRuntimeProfileFragment) != std::string::npos,
                "radius health runtime_profile missing") &&
         ok;

    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_INET;
    std::string username = "radius_user";
    std::string client_addr = "127.0.0.42";
    std::string server_addr = "127.0.0.1";
    conn.username = toSlice(username);
    conn.client_address = toSlice(client_addr);
    conn.server_address = toSlice(server_addr);

    sb_auth_step_result_v1 result{};

    // Hardening H1: test directives denied by default unless policy enables them.
    const std::string good_auth = computeAuthenticator("radius_user", "goodpass", "radius-secret-v1");
    const std::string failover_payload =
        "password=goodpass;auth=" + good_auth + ";simulate=timeout:radius-primary";
    rc = begin(api, instance, &conn, failover_payload, &result);
    ok = expect(rc == SB_AUTH_RC_POLICY_VIOLATION,
                "simulate directive should be denied when policy is disabled") &&
         ok;
    ok = expect(std::string(result.plugin_error_code) == kErrTestDirectiveDenied,
                "test-directive deny error code mismatch") &&
         ok;

    g_policy_store[kPolicyAllowTestDirectives] = "true";
    std::memset(&result, 0, sizeof(result));
    rc = begin(api, instance, &conn, failover_payload, &result);
    ok = expect(rc == SB_AUTH_RC_POLICY_VIOLATION,
                "simulate directive should remain denied under production profile") &&
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
        "{\"radius_servers\":[\"radius-primary\",\"radius-secondary\"],\"shared_secret_ref\":\"auth.radius.shared_secret_ref\",\"request_timeout_ms\":1000,\"runtime_profile\":\"test\",\"allowed_radius_endpoints\":[\"radius-primary\",\"radius-secondary\"]}";
    ok = expect(api->configure_instance(test_profile_instance, toSlice(config_json_test_profile)) ==
                    SB_AUTH_RC_OK,
                "configure_instance for test profile failed") &&
         ok;

    std::memset(&result, 0, sizeof(result));
    rc = begin(api, test_profile_instance, &conn, failover_payload, &result);
    ok = expect(rc == SB_AUTH_RC_OK,
                "primary timeout should fail over to secondary success in test profile") &&
         ok;

    // AT-B11-02: bad shared secret/authenticator denied.
    std::memset(&result, 0, sizeof(result));
    const std::string bad_auth = computeAuthenticator("radius_user", "goodpass", "wrong-secret");
    const std::string bad_secret_payload =
        "password=goodpass;auth=" + bad_auth;
    rc = begin(api, instance, &conn, bad_secret_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "bad shared secret should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrSecretInvalid,
                "bad secret error code mismatch") &&
         ok;

    // AT-B11-02: explicit RADIUS reject denied.
    std::memset(&result, 0, sizeof(result));
    const std::string reject_payload =
        "password=goodpass;auth=" + good_auth + ";simulate=reject:radius-primary";
    rc = begin(api, test_profile_instance, &conn, reject_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "radius reject should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrRejected,
                "radius reject error code mismatch") &&
         ok;

    // AT-B11-03: unknown payload key denied (strict schema).
    std::memset(&result, 0, sizeof(result));
    const std::string unknown_key_payload =
        "password=goodpass;auth=" + good_auth + ";unexpected=1";
    rc = begin(api, instance, &conn, unknown_key_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "unknown payload key should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "unknown payload key error code mismatch") &&
         ok;

    // AT-B11-04: duplicate authenticator aliases denied (no overwrite semantics).
    std::memset(&result, 0, sizeof(result));
    const std::string duplicate_auth_payload =
        "password=goodpass;auth=" + good_auth + ";mac=" + good_auth;
    rc = begin(api, instance, &conn, duplicate_auth_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "duplicate authenticator aliases should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "duplicate authenticator aliases error code mismatch") &&
         ok;

    // AT-B11-05: raw payload denied (must be key-value schema).
    std::memset(&result, 0, sizeof(result));
    rc = begin(api, instance, &conn, "goodpass", &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "raw payload should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "raw payload error code mismatch") &&
         ok;

    ok = expect(g_audit_events >= 7, "expected audit events for radius begin calls") && ok;

    api->destroy_instance(test_profile_instance);
    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "radius_plugin_selftest: PASS\n";
    return 0;
}
