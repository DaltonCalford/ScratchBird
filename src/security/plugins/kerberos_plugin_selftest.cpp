/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "scratchbird/security/auth_plugin_abi_v1.h"
#include "auth_plugin_crypto.h"

#include <dlfcn.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.kerberos";
constexpr char kMethodKerberosGssApi[] = "scratchbird.auth.kerberos_gssapi";

constexpr char kPolicyKeytabRef[] = "auth.kerberos.keytab_ref";

constexpr char kErrReplayDetected[] = "AUTH_KERBEROS_REPLAY_DETECTED";
constexpr char kErrTicketInvalid[] = "AUTH_KERBEROS_TICKET_INVALID";
constexpr char kErrTicketExpired[] = "AUTH_KERBEROS_TICKET_EXPIRED";
constexpr char kErrMalformedPayload[] = "AUTH_KERBEROS_PAYLOAD_MALFORMED";
constexpr char kHealthDriverFragment[] = "\"provider_driver\":\"kerberos_native\"";
constexpr char kHealthRuntimeProfileFragment[] = "\"runtime_profile\":\"production\"";

std::unordered_map<std::string, std::string> g_policy_store{
    {kPolicyKeytabRef, "krb5-keytab-v1"},
};
uint64_t g_audit_events = 0;

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

uint64_t nowUnixMs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string makeTicket(const std::string& principal,
                       const std::string& subject,
                       uint64_t ts_unix_ms,
                       const std::string& nonce,
                       const std::string& kdc,
                       const std::string& keytab_secret) {
    const std::string sig_payload = principal + "|" + subject + "|" +
                                    std::to_string(ts_unix_ms) + "|" + nonce + "|" + kdc;
    std::string sig;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(keytab_secret,
                                                                sig_payload,
                                                                &sig)) {
        return "";
    }
    return "princ=" + principal + ";sub=" + subject + ";ts=" + std::to_string(ts_unix_ms) +
           ";nonce=" + nonce + ";kdc=" + kdc + ";sig=" + sig;
}

sb_auth_slice_t toSlice(const std::string& value) {
    return sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(value.data()),
        static_cast<uint32_t>(value.size())
    };
}

std::string toSoPath(const std::string& plugin_root) {
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_kerberos.so";
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

sb_auth_rc_t resolveUserByExternalSubject(sb_auth_slice_t issuer,
                                          sb_auth_slice_t subject,
                                          uint8_t out_user_uuid[SB_AUTH_UUID_BYTES]) {
    if (!issuer.ptr || issuer.len == 0 || !subject.ptr || subject.len == 0 || !out_user_uuid) {
        return SB_AUTH_RC_DENY;
    }
    std::memset(out_user_uuid, 0, SB_AUTH_UUID_BYTES);
    out_user_uuid[0] = 0xA8;
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
                           toSlice(std::string(kMethodKerberosGssApi)),
                           conn,
                           toSlice(payload),
                           &exchange,
                           out_result);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: kerberos_plugin_selftest <auth_plugin_output_root>\n";
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
    host_api.now_unix_ms = &nowUnixMs;
    host_api.read_policy_value = &readPolicyValue;
    host_api.emit_audit_event = &emitAuditEvent;
    host_api.resolve_user_by_external_subject = &resolveUserByExternalSubject;
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
        "{\"service_principal\":\"postgres/host@example.com\",\"keytab_path\":\"policy:auth.kerberos.keytab_ref\",\"max_replay_window_ms\":5000,\"allowed_kdc_endpoints\":[\"127.0.0.1\"]}";
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
                "kerberos health provider_driver missing") &&
         ok;
    ok = expect(health_text.find(kHealthRuntimeProfileFragment) != std::string::npos,
                "kerberos health runtime_profile missing") &&
         ok;

    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_INET;
    std::string username = "kerberos_user";
    std::string client_addr = "127.0.0.42";
    std::string server_addr = "127.0.0.1";
    conn.username = toSlice(username);
    conn.client_address = toSlice(client_addr);
    conn.server_address = toSlice(server_addr);

    sb_auth_step_result_v1 result{};

    // AT-B14-01: valid ticket accepted.
    const uint64_t issued_ms = nowUnixMs();
    const std::string valid_ticket = makeTicket(
        "postgres/host@example.com",
        "sub:krb-user",
        issued_ms,
        "nonce-1",
        "127.0.0.1",
        "krb5-keytab-v1");
    rc = begin(api, instance, &conn, valid_ticket, &result);
    ok = expect(rc == SB_AUTH_RC_OK, "valid kerberos ticket should allow") && ok;

    // AT-B14-02: replay denied.
    std::memset(&result, 0, sizeof(result));
    rc = begin(api, instance, &conn, valid_ticket, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "replayed kerberos ticket should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrReplayDetected,
                "replay error code mismatch") &&
         ok;

    // AT-B14-02: invalid ticket denied.
    std::memset(&result, 0, sizeof(result));
    const std::string invalid_ticket = makeTicket(
        "postgres/host@example.com",
        "sub:krb-user",
        nowUnixMs(),
        "nonce-2",
        "127.0.0.1",
        "wrong-keytab");
    rc = begin(api, instance, &conn, invalid_ticket, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "invalid kerberos ticket should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrTicketInvalid,
                "invalid ticket error code mismatch") &&
         ok;

    // AT-B14-02: expired ticket denied.
    std::memset(&result, 0, sizeof(result));
    const std::string expired_ticket = makeTicket(
        "postgres/host@example.com",
        "sub:krb-user",
        nowUnixMs() - 60000,
        "nonce-expired",
        "127.0.0.1",
        "krb5-keytab-v1");
    rc = begin(api, instance, &conn, expired_ticket, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "expired kerberos ticket should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrTicketExpired,
                "expired ticket error code mismatch") &&
         ok;

    // AT-B14-03: unknown payload key denied (strict schema).
    std::memset(&result, 0, sizeof(result));
    const std::string unknown_key_ticket = valid_ticket + ";extra=1";
    rc = begin(api, instance, &conn, unknown_key_ticket, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "unknown ticket field should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "unknown ticket field error code mismatch") &&
         ok;

    // AT-B14-04: duplicate principal aliases denied (no overwrite semantics).
    std::memset(&result, 0, sizeof(result));
    const std::string duplicate_principal_ticket =
        valid_ticket + ";principal=postgres/host@example.com";
    rc = begin(api, instance, &conn, duplicate_principal_ticket, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "duplicate principal aliases should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "duplicate principal aliases error code mismatch") &&
         ok;

    // AT-B14-05: malformed signature shape denied.
    std::memset(&result, 0, sizeof(result));
    const std::string malformed_sig_ticket =
        "princ=postgres/host@example.com;sub=sub:krb-user;ts=" + std::to_string(nowUnixMs()) +
        ";nonce=nonce-3;kdc=127.0.0.1;sig=nothex";
    rc = begin(api, instance, &conn, malformed_sig_ticket, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "malformed signature should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "malformed signature error code mismatch") &&
         ok;

    ok = expect(g_audit_events >= 7, "expected audit events for kerberos begin calls") && ok;

    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "kerberos_plugin_selftest: PASS\n";
    return 0;
}
