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

constexpr char kPluginId[] = "scratchbird.auth.peer";
constexpr char kMethodPeerUid[] = "scratchbird.auth.peer_uid";
constexpr char kPolicyAcceptIpc[] = "auth.peer.accept_ipc";
constexpr char kPolicyAllowUidZero[] = "auth.peer.allow_uid_zero";
constexpr char kErrRemoteTransport[] = "AUTH_PEER_REMOTE_TRANSPORT";
constexpr char kErrIpcDisabled[] = "AUTH_PEER_IPC_DISABLED";
constexpr char kErrRootUidDenied[] = "AUTH_PEER_UID_ZERO_DENIED";
constexpr char kErrPeerPidMissing[] = "AUTH_PEER_PID_MISSING";
constexpr char kErrPayloadNotEmpty[] = "AUTH_PEER_PAYLOAD_NOT_EMPTY";

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
    out_user_uuid[0] = 0x33;
    return SB_AUTH_RC_OK;
}

sb_auth_slice_t toSlice(const std::string& value) {
    return sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(value.data()),
        static_cast<uint32_t>(value.size())
    };
}

std::string toSoPath(const std::string& plugin_root) {
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_peer.so";
}

sb_auth_rc_t runBegin(const sb_auth_plugin_api_v1* api,
                      sb_auth_plugin_instance_t instance,
                      sb_auth_transport_t transport,
                      uint32_t peer_uid,
                      uint32_t peer_pid,
                      const std::string& payload_text,
                      sb_auth_step_result_v1* out_result) {
    std::string username = "peer_user";
    sb_auth_exchange_t exchange = 0;

    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = transport;
    conn.username = toSlice(username);
    conn.peer_uid = peer_uid;
    conn.peer_gid = 1000;
    conn.peer_pid = peer_pid;

    return api->begin_auth(instance,
                           toSlice(std::string(kMethodPeerUid)),
                           &conn,
                           toSlice(payload_text),
                           &exchange,
                           out_result);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: peer_plugin_selftest <auth_plugin_output_root>\n";
        return 2;
    }

    g_policy_store[kPolicyAcceptIpc] = "true";
    g_policy_store[kPolicyAllowUidZero] = "false";

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

    sb_auth_step_result_v1 result{};

    // Non-empty payload is invalid for peer auth.
    std::memset(&result, 0, sizeof(result));
    sb_auth_rc_t rc = runBegin(api,
                               instance,
                               SB_AUTH_TRANSPORT_LOCAL,
                               1000,
                               2000,
                               std::string("ignored"),
                               &result);
    ok = expect(rc == SB_AUTH_RC_INVALID_ARGUMENT, "non-empty payload should be rejected") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrPayloadNotEmpty,
                "payload reject error code mismatch") &&
         ok;

    // INET transport is always denied.
    std::memset(&result, 0, sizeof(result));
    rc = runBegin(api, instance, SB_AUTH_TRANSPORT_INET, 1000, 2000, std::string(), &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "INET transport should be denied") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrRemoteTransport,
                "INET deny error code mismatch") &&
         ok;

    // LOCAL with valid peer creds should pass.
    std::memset(&result, 0, sizeof(result));
    rc = runBegin(api, instance, SB_AUTH_TRANSPORT_LOCAL, 1000, 2001, std::string(), &result);
    ok = expect(rc == SB_AUTH_RC_OK, "LOCAL transport with valid creds should allow") && ok;

    // UID=0 denied unless explicitly allowed by policy.
    std::memset(&result, 0, sizeof(result));
    rc = runBegin(api, instance, SB_AUTH_TRANSPORT_LOCAL, 0, 2002, std::string(), &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "UID zero should be denied when policy disallows") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrRootUidDenied,
                "UID zero deny error code mismatch") &&
         ok;

    // IPC can be disabled by policy.
    g_policy_store[kPolicyAcceptIpc] = "false";
    std::memset(&result, 0, sizeof(result));
    rc = runBegin(api, instance, SB_AUTH_TRANSPORT_IPC, 1000, 2003, std::string(), &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "IPC transport should be denied when policy disallows") &&
         ok;
    ok = expect(std::string(result.plugin_error_code) == kErrIpcDisabled,
                "IPC policy deny error code mismatch") &&
         ok;
    g_policy_store[kPolicyAcceptIpc] = "true";

    // Missing PID is rejected.
    std::memset(&result, 0, sizeof(result));
    rc = runBegin(api, instance, SB_AUTH_TRANSPORT_LOCAL, 1000, 0, std::string(), &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "missing peer PID should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrPeerPidMissing,
                "missing PID error code mismatch") &&
         ok;

    ok = expect(g_audit_events >= 5, "expected audit emission across peer test cases") && ok;

    api->destroy_instance(instance);
    dlclose(handle);

    if (!ok) {
        return 1;
    }
    std::cout << "peer_plugin_selftest: PASS\n";
    return 0;
}
