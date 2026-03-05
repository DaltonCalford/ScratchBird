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

namespace {

constexpr char kPluginId[] = "scratchbird.auth.ident";
constexpr char kMethodIdentRfc1413[] = "scratchbird.auth.ident_rfc1413";

constexpr char kErrAddressMismatch[] = "AUTH_IDENT_ADDRESS_MISMATCH";
constexpr char kErrUsernameMismatch[] = "AUTH_CREDENTIAL_INVALID";
constexpr char kErrQueryFailed[] = "AUTH_IDENT_QUERY_FAILED";
constexpr char kErrUntrustedTransport[] = "AUTH_IDENT_UNTRUSTED_TRANSPORT";
constexpr char kHealthDriverFragment[] = "\"provider_driver\":\"ident_native\"";
constexpr char kHealthRuntimeProfileFragment[] = "\"runtime_profile\":\"production\"";

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
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_ident.so";
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
    const std::string name(reinterpret_cast<const char*>(username.ptr),
                           reinterpret_cast<const char*>(username.ptr) + username.len);
    if (name != "dbuser") {
        return SB_AUTH_RC_DENY;
    }
    std::memset(out_user_uuid, 0, SB_AUTH_UUID_BYTES);
    out_user_uuid[0] = 0x81;
    return SB_AUTH_RC_OK;
}

sb_auth_rc_t begin(const sb_auth_plugin_api_v1* api,
                   sb_auth_plugin_instance_t instance,
                   sb_auth_connection_ctx_v1* conn,
                   const std::string& payload,
                   sb_auth_step_result_v1* out_result) {
    sb_auth_exchange_t exchange = 0;
    return api->begin_auth(instance,
                           toSlice(std::string(kMethodIdentRfc1413)),
                           conn,
                           toSlice(payload),
                           &exchange,
                           out_result);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: ident_plugin_selftest <auth_plugin_output_root>\n";
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
        "{\"trusted_cidrs\":[\"127.0.0.0/24\"],\"ident_timeout_ms\":1000,\"require_username_match\":true}";
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
                "ident health provider_driver missing") &&
         ok;
    ok = expect(health_text.find(kHealthRuntimeProfileFragment) != std::string::npos,
                "ident health runtime_profile missing") &&
         ok;

    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_INET;
    std::string username = "dbuser";
    std::string client_addr = "127.0.0.42";
    std::string server_addr = "127.0.0.1";
    conn.username = toSlice(username);
    conn.client_address = toSlice(client_addr);
    conn.server_address = toSlice(server_addr);

    sb_auth_step_result_v1 result{};

    // AT-B10-01: trusted CIDR + matching user accepted.
    std::string valid_payload = "ident_user=dbuser;client_addr=127.0.0.42";
    rc = begin(api, instance, &conn, valid_payload, &result);
    ok = expect(rc == SB_AUTH_RC_OK, "trusted cidr + matching user should allow") && ok;

    // AT-B10-02: untrusted client transport denied by CIDR policy.
    std::memset(&result, 0, sizeof(result));
    std::string untrusted_client_addr = "10.0.0.42";
    conn.client_address = toSlice(untrusted_client_addr);
    std::string untrusted_payload = "ident_user=dbuser;client_addr=10.0.0.42";
    rc = begin(api, instance, &conn, untrusted_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "untrusted client address should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrUntrustedTransport,
                "untrusted transport error code mismatch") &&
         ok;
    conn.client_address = toSlice(client_addr);

    // AT-B10-02: spoofed client address denied.
    std::memset(&result, 0, sizeof(result));
    std::string spoof_payload = "ident_user=dbuser;client_addr=127.0.0.99";
    rc = begin(api, instance, &conn, spoof_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "spoofed client address should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrAddressMismatch,
                "spoofed address error code mismatch") &&
         ok;

    // AT-B10-02: username mismatch denied.
    std::memset(&result, 0, sizeof(result));
    std::string mismatch_payload = "ident_user=otheruser;client_addr=127.0.0.42";
    rc = begin(api, instance, &conn, mismatch_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "username mismatch should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrUsernameMismatch,
                "username mismatch error code mismatch") &&
         ok;

    // AT-B10-03: unknown payload key denied (fail-closed schema).
    std::memset(&result, 0, sizeof(result));
    std::string unknown_key_payload = "ident_user=dbuser;unknown=1;client_addr=127.0.0.42";
    rc = begin(api, instance, &conn, unknown_key_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "unknown payload key should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrQueryFailed,
                "unknown payload key error code mismatch") &&
         ok;

    // AT-B10-04: duplicate user aliases denied (no overwrite semantics).
    std::memset(&result, 0, sizeof(result));
    std::string duplicate_user_payload = "ident_user=dbuser;user=dbuser;client_addr=127.0.0.42";
    rc = begin(api, instance, &conn, duplicate_user_payload, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "duplicate user aliases should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrQueryFailed,
                "duplicate user aliases error code mismatch") &&
         ok;

    // AT-B10-05: raw username payload denied (must be key-value schema).
    std::memset(&result, 0, sizeof(result));
    rc = begin(api, instance, &conn, "dbuser", &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "raw username payload should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrQueryFailed,
                "raw username payload error code mismatch") &&
         ok;

    ok = expect(g_audit_events >= 7, "expected audit events for ident begin calls") && ok;

    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "ident_plugin_selftest: PASS\n";
    return 0;
}
