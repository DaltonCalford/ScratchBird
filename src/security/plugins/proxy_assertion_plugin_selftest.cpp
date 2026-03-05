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

constexpr char kPluginId[] = "scratchbird.auth.proxy_assertion";
constexpr char kMethodProxyPrincipalAssertion[] = "scratchbird.auth.proxy_principal_assertion";

constexpr char kPolicySigningKeyRef[] = "auth.proxy_assertion.signing_key_ref";
constexpr char kPolicyAllowInet[] = "auth.proxy_assertion.allow_inet";
constexpr char kPolicyTrustedProxyUid[] = "auth.proxy_assertion.trusted_proxy_uid";
constexpr char kPolicyExpectedIssuer[] = "auth.proxy_assertion.expected_issuer";
constexpr char kPolicyRequiredAudience[] = "auth.proxy_assertion.required_audience";
constexpr char kPolicyExpectedProxyId[] = "auth.proxy_assertion.expected_proxy_id";

constexpr char kErrMalformedPayload[] = "AUTH_PROXY_ASSERTION_PAYLOAD_MALFORMED";
constexpr char kErrSignatureInvalid[] = "AUTH_PROXY_ASSERTION_SIGNATURE_INVALID";
constexpr char kErrUntrustedProxy[] = "AUTH_PROXY_ASSERTION_UNTRUSTED_PROXY";

std::unordered_map<std::string, std::string> g_policy_store{
    {kPolicySigningKeyRef, "proxy-signing-key-v1"},
    {kPolicyAllowInet, "false"},
    {kPolicyTrustedProxyUid, "1000"},
    {kPolicyExpectedIssuer, "proxy"},
    {kPolicyRequiredAudience, "scratchbird"},
    {kPolicyExpectedProxyId, "edge-proxy-1"},
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
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_proxy_assertion.so";
}

uint64_t nowUnixMs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string makeAssertion(const std::string& issuer,
                          const std::string& subject,
                          const std::string& audience,
                          const std::string& proxy_id,
                          uint64_t exp_unix_ms,
                          const std::string& signing_key) {
    const std::string signature_payload = issuer + "|" + subject + "|" + audience + "|" +
                                          proxy_id + "|" + std::to_string(exp_unix_ms);
    std::string sig;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(signing_key,
                                                                signature_payload,
                                                                &sig)) {
        return "";
    }
    return "iss=" + issuer + ";sub=" + subject + ";aud=" + audience + ";proxy=" + proxy_id +
           ";exp=" + std::to_string(exp_unix_ms) + ";sig=" + sig;
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
    out_user_uuid[0] = 0x36;
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
                           toSlice(std::string(kMethodProxyPrincipalAssertion)),
                           conn,
                           toSlice(payload),
                           &exchange,
                           out_result);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: proxy_assertion_plugin_selftest <auth_plugin_output_root>\n";
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
    ok = expect(api->configure_instance(instance, sb_auth_slice_t{}) == SB_AUTH_RC_OK,
                "configure_instance failed") &&
         ok;
    ok = expect(api->configure_instance(instance, toSlice(std::string("{}"))) == SB_AUTH_RC_UNSUPPORTED,
                "non-empty config should be unsupported") &&
         ok;

    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_LOCAL;
    std::string username = "proxy_user";
    conn.username = toSlice(username);
    conn.peer_uid = 1000;
    conn.peer_gid = 1000;
    conn.peer_pid = 4242;

    sb_auth_step_result_v1 result{};

    // AT-B08-01: trusted proxy assertion accepted.
    const std::string valid_assertion = makeAssertion(
        "proxy",
        "sub:proxied-user",
        "scratchbird",
        "edge-proxy-1",
        nowUnixMs() + 60000,
        "proxy-signing-key-v1");
    sb_auth_rc_t rc = begin(api, instance, &conn, valid_assertion, &result);
    ok = expect(rc == SB_AUTH_RC_OK, "trusted signed proxy assertion should allow") && ok;

    // H2: unknown payload fields must fail closed.
    std::memset(&result, 0, sizeof(result));
    const std::string malformed_assertion = valid_assertion + ";tenant=blue";
    rc = begin(api, instance, &conn, malformed_assertion, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "unknown payload fields should deny as malformed") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedPayload,
                "unknown-field malformed payload error code mismatch") &&
         ok;

    // AT-B08-02: unsigned/invalid signature assertion denied.
    std::memset(&result, 0, sizeof(result));
    const std::string bad_sig_assertion = makeAssertion(
        "proxy",
        "sub:proxied-user",
        "scratchbird",
        "edge-proxy-1",
        nowUnixMs() + 60000,
        "wrong-signing-key");
    rc = begin(api, instance, &conn, bad_sig_assertion, &result);
    ok = expect(rc == SB_AUTH_RC_SIGNATURE_INVALID,
                "assertion with invalid signature must be denied") &&
         ok;
    ok = expect(std::string(result.plugin_error_code) == kErrSignatureInvalid,
                "invalid signature error code mismatch") &&
         ok;

    // AT-B08-02: untrusted proxy boundary denied.
    std::memset(&result, 0, sizeof(result));
    conn.peer_uid = 2000;
    rc = begin(api, instance, &conn, valid_assertion, &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "untrusted proxy uid should deny assertion") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrUntrustedProxy,
                "untrusted proxy error code mismatch") &&
         ok;

    ok = expect(g_audit_events >= 4, "expected audit events for proxy begin calls") && ok;

    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "proxy_assertion_plugin_selftest: PASS\n";
    return 0;
}
