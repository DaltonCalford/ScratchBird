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

constexpr char kPluginId[] = "scratchbird.auth.certificate_mtls";
constexpr char kMethodCertificateX509[] = "scratchbird.auth.certificate_x509";
constexpr char kPolicyRequiredSanPrefix[] = "auth.certificate_mtls.required_san_prefix";

constexpr char kErrMalformedSubject[] = "AUTH_CERT_SUBJECT_MALFORMED";
constexpr char kErrChainUntrusted[] = "AUTH_CERT_CHAIN_UNTRUSTED";
constexpr char kErrRevoked[] = "AUTH_CERT_REVOKED";

std::unordered_map<std::string, std::string> g_policy_store{
    {kPolicyRequiredSanPrefix, "dns:db.example.com"},
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
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_certificate_mtls.so";
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
    out_user_uuid[0] = 0xAB;
    return SB_AUTH_RC_OK;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: certificate_mtls_plugin_selftest <auth_plugin_output_root>\n";
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
    host_api.resolve_user_by_external_subject = &resolveUserByExternalSubject;

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

    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_LOCAL;
    std::string username = "mtls_user";
    conn.username = toSlice(username);
    conn.peer_uid = 1000;
    conn.peer_gid = 1000;
    conn.peer_pid = 444;

    sb_auth_exchange_t exchange = 0;
    sb_auth_step_result_v1 result{};

    // AT-B05-01: trusted chain + SAN policy pass.
    std::string valid_payload =
        "sub=CN=db_user;san=dns:db.example.com;chain=trusted;revoked=0";
    sb_auth_rc_t rc = api->begin_auth(instance,
                                      toSlice(std::string(kMethodCertificateX509)),
                                      &conn,
                                      toSlice(valid_payload),
                                      &exchange,
                                      &result);
    ok = expect(rc == SB_AUTH_RC_OK, "valid chain + SAN should allow") && ok;

    // H2: unknown fields are malformed input.
    std::memset(&result, 0, sizeof(result));
    std::string malformed_unknown_field_payload =
        "sub=CN=db_user;san=dns:db.example.com;chain=trusted;revoked=0;role=dba";
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodCertificateX509)),
                         &conn,
                         toSlice(malformed_unknown_field_payload),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "unknown payload fields should be malformed") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedSubject,
                "unknown payload field malformed error code mismatch") &&
         ok;

    // AT-B05-02: untrusted chain denied.
    std::memset(&result, 0, sizeof(result));
    std::string untrusted_payload =
        "sub=CN=db_user;san=dns:db.example.com;chain=untrusted;revoked=0";
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodCertificateX509)),
                         &conn,
                         toSlice(untrusted_payload),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "untrusted chain should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrChainUntrusted,
                "untrusted chain error code mismatch") &&
         ok;

    // AT-B05-02: revoked certificate denied.
    std::memset(&result, 0, sizeof(result));
    std::string revoked_payload =
        "sub=CN=db_user;san=dns:db.example.com;chain=trusted;revoked=1";
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodCertificateX509)),
                         &conn,
                         toSlice(revoked_payload),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "revoked certificate should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrRevoked,
                "revoked certificate error code mismatch") &&
         ok;

    ok = expect(g_audit_events >= 4, "expected audit events for mtls begin calls") && ok;

    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "certificate_mtls_plugin_selftest: PASS\n";
    return 0;
}
