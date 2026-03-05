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

constexpr char kPluginId[] = "scratchbird.auth.trust_reject";
constexpr char kMethodTrust[] = "scratchbird.auth.trust";
constexpr char kMethodReject[] = "scratchbird.auth.reject";
constexpr char kPolicyTrustEnabled[] = "auth.trust_reject.trust_enabled";
constexpr char kErrTrustDisabled[] = "AUTH_TRUST_REJECT_TRUST_DISABLED";
constexpr char kErrForcedReject[] = "AUTH_TRUST_REJECT_FORCED_DENY";
constexpr char kErrMalformedPayload[] = "AUTH_TRUST_REJECT_PAYLOAD_INVALID";

std::unordered_map<std::string, std::string> g_policy_store;
uint64_t g_audit_events = 0;

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
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
    out_user_uuid[0] = 0x51;
    return SB_AUTH_RC_OK;
}

sb_auth_slice_t toSlice(const std::string& value) {
    return sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(value.data()),
        static_cast<uint32_t>(value.size())
    };
}

std::string toSoPath(const std::string& plugin_root) {
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_trust_reject.so";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: trust_reject_plugin_selftest <auth_plugin_output_root>\n";
        return 2;
    }

    const std::string plugin_path = toSoPath(argv[1]);
    void* handle = dlopen(plugin_path.c_str(), RTLD_NOW | RTLD_LOCAL);
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
    ok = expect(api->configure_instance(instance, sb_auth_slice_t{}) == SB_AUTH_RC_OK,
                "configure_instance failed") &&
         ok;
    ok = expect(api->configure_instance(instance, toSlice(std::string("{}"))) == SB_AUTH_RC_UNSUPPORTED,
                "non-empty config should be unsupported") &&
         ok;

    std::string username = "selftest_user";
    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_LOCAL;
    conn.username = toSlice(username);
    conn.peer_uid = 1000;
    conn.peer_gid = 1000;
    conn.peer_pid = 2222;

    std::string payload_text;
    sb_auth_slice_t payload = toSlice(payload_text);
    sb_auth_exchange_t exchange = 0;
    sb_auth_step_result_v1 result{};

    // Strict payload schema should reject free-form payload text.
    std::string malformed_payload_text = "ignored";
    const sb_auth_rc_t malformed_payload_rc = api->begin_auth(
        instance,
        toSlice(std::string(kMethodTrust)),
        &conn,
        toSlice(malformed_payload_text),
        &exchange,
        &result);
    ok = expect(malformed_payload_rc == SB_AUTH_RC_INVALID_ARGUMENT,
                "free-form payload must be rejected") &&
         ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "malformed payload error code mismatch") &&
         ok;
    ok = expect(result.plugin_error_numeric == 1006,
                "malformed payload error numeric mismatch") &&
         ok;

    // Trust path enabled via policy should allow.
    g_policy_store[kPolicyTrustEnabled] = "true";
    std::string valid_payload_text = "reason=healthcheck;request_id=req-42";
    const sb_auth_rc_t trust_ok_rc = api->begin_auth(
        instance,
        toSlice(std::string(kMethodTrust)),
        &conn,
        toSlice(valid_payload_text),
        &exchange,
        &result);
    ok = expect(trust_ok_rc == SB_AUTH_RC_OK,
                "trust path should allow when policy trust_enabled=true") &&
         ok;

    // Trust path disabled should fail closed with deterministic policy error code.
    g_policy_store[kPolicyTrustEnabled] = "false";
    std::memset(&result, 0, sizeof(result));
    const sb_auth_rc_t trust_deny_rc = api->begin_auth(
        instance,
        toSlice(std::string(kMethodTrust)),
        &conn,
        payload,
        &exchange,
        &result);
    ok = expect(trust_deny_rc == SB_AUTH_RC_DENY,
                "trust path should deny when policy trust_enabled=false") &&
         ok;
    ok = expect(std::string(result.plugin_error_code) == kErrTrustDisabled,
                "trust disabled error code mismatch") &&
         ok;
    ok = expect(result.plugin_error_numeric == 1005,
                "trust disabled error numeric mismatch") &&
         ok;

    // Reject path remains deterministic independent of policy.
    g_policy_store[kPolicyTrustEnabled] = "true";
    std::memset(&result, 0, sizeof(result));
    const sb_auth_rc_t reject_rc = api->begin_auth(
        instance,
        toSlice(std::string(kMethodReject)),
        &conn,
        payload,
        &exchange,
        &result);
    ok = expect(reject_rc == SB_AUTH_RC_DENY, "reject path should deny deterministically") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrForcedReject,
                "reject error code mismatch") &&
         ok;
    ok = expect(result.plugin_error_numeric == 1001,
                "reject error numeric mismatch") &&
         ok;

    ok = expect(g_audit_events >= 3, "expected audit emission for trust/reject begin paths") && ok;

    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }
    std::cout << "trust_reject_plugin_selftest: PASS\n";
    return 0;
}
