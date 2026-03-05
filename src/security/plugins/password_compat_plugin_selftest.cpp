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

constexpr char kPluginId[] = "scratchbird.auth.password_compat";
constexpr char kMethodPasswordCompat[] = "scratchbird.auth.password_compat";
constexpr char kMethodMd5Legacy[] = "scratchbird.auth.md5_legacy";

constexpr char kPolicyMode[] = "auth.password_compat.mode";
constexpr char kPolicyAllowMd5[] = "auth.password_compat.allow_md5_legacy";
constexpr char kPolicyCredentialRefUser[] =
    "auth.password_compat.credential_ref.selftest_user";

constexpr char kErrMd5Denied[] = "AUTH_PASSWORD_MD5_LEGACY_DENIED";
constexpr char kErrCredentialMismatch[] = "AUTH_PASSWORD_CREDENTIAL_MISMATCH";

std::unordered_map<std::string, std::string> g_policy_store;
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
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_password_compat.so";
}

sb_auth_rc_t readPolicyValue(sb_auth_slice_t key, sb_auth_slice_t* out_value) {
    if (!out_value || !key.ptr || key.len == 0) {
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
    out_user_uuid[0] = 0x64;
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
                   const std::string& method,
                   const std::string& payload,
                   sb_auth_step_result_v1* out_result) {
    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_LOCAL;
    std::string username = "selftest_user";
    conn.username = toSlice(username);
    conn.peer_uid = 1000;
    conn.peer_gid = 1000;
    conn.peer_pid = 777;

    sb_auth_exchange_t exchange = 0;
    return api->begin_auth(instance,
                           toSlice(method),
                           &conn,
                           toSlice(payload),
                           &exchange,
                           out_result);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: password_compat_plugin_selftest <auth_plugin_output_root>\n";
        return 2;
    }

    g_policy_store[kPolicyMode] = "bootstrap_allow";
    g_policy_store[kPolicyAllowMd5] = "false";
    g_policy_store[kPolicyCredentialRefUser] = "literal:compat-secret";

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
    ok = expect(api->configure_instance(instance, sb_auth_slice_t{}) == SB_AUTH_RC_OK,
                "configure_instance failed") &&
         ok;
    ok = expect(api->configure_instance(instance, toSlice(std::string("{}"))) == SB_AUTH_RC_UNSUPPORTED,
                "non-empty config should be unsupported") &&
         ok;

    sb_auth_step_result_v1 result{};

    // AT-B03-01: compat success validates credential store-backed secret.
    std::memset(&result, 0, sizeof(result));
    sb_auth_rc_t rc = begin(api, instance, kMethodPasswordCompat, "compat-secret", &result);
    ok = expect(rc == SB_AUTH_RC_OK, "compat path should allow with matching stored secret") && ok;

    // Mismatch should deny deterministically.
    std::memset(&result, 0, sizeof(result));
    rc = begin(api, instance, kMethodPasswordCompat, "wrong-secret", &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "compat path should deny on secret mismatch") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrCredentialMismatch,
                "compat mismatch error code mismatch") &&
         ok;

    // AT-B03-02: md5 legacy rejected per policy.
    std::memset(&result, 0, sizeof(result));
    g_policy_store[kPolicyAllowMd5] = "false";
    rc = begin(api, instance, kMethodMd5Legacy, "compat-secret", &result);
    ok = expect(rc == SB_AUTH_RC_POLICY_VIOLATION,
                "md5 legacy should be denied when policy disallows") &&
         ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMd5Denied,
                "md5 policy deny error code mismatch") &&
         ok;

    // Migration control: md5 can be temporarily enabled by policy.
    std::memset(&result, 0, sizeof(result));
    g_policy_store[kPolicyAllowMd5] = "true";
    rc = begin(api, instance, kMethodMd5Legacy, "compat-secret", &result);
    ok = expect(rc == SB_AUTH_RC_OK,
                "md5 legacy should allow when policy explicitly enables migration") &&
         ok;

    ok = expect(g_audit_events >= 4, "expected audit events for tested begin calls") && ok;

    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "password_compat_plugin_selftest: PASS\n";
    return 0;
}
