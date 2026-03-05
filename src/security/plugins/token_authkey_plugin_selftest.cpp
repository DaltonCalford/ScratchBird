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

constexpr char kPluginId[] = "scratchbird.auth.token_authkey";
constexpr char kMethodAuthKeyToken[] = "scratchbird.auth.authkey_token";

constexpr char kPolicySigningKeyRef[] = "auth.token_authkey.signing_key_ref";
constexpr char kPolicyExpectedIssuer[] = "auth.token_authkey.expected_issuer";
constexpr char kPolicyExpectedAudience[] = "auth.token_authkey.expected_audience";

constexpr char kErrMalformedToken[] = "AUTH_TOKEN_MALFORMED";
constexpr char kErrExpired[] = "AUTH_TOKEN_EXPIRED";
constexpr char kErrAudienceMismatch[] = "AUTH_TOKEN_AUDIENCE_MISMATCH";

std::unordered_map<std::string, std::string> g_policy_store{
    {kPolicySigningKeyRef, "token-signing-key"},
    {kPolicyExpectedIssuer, "scratchbird.auth.authkey"},
    {kPolicyExpectedAudience, "scratchbird"},
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

std::string tokenFor(const std::string& iss,
                     const std::string& aud,
                     const std::string& sub,
                     uint64_t exp_unix_ms,
                     const std::string& key) {
    const std::string payload =
        iss + "|" + aud + "|" + sub + "|" + std::to_string(exp_unix_ms);
    std::string sig;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(key, payload, &sig)) {
        return "";
    }
    return "iss=" + iss + ";aud=" + aud + ";sub=" + sub + ";exp=" +
           std::to_string(exp_unix_ms) + ";sig=" + sig;
}

sb_auth_slice_t toSlice(const std::string& value) {
    return sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(value.data()),
        static_cast<uint32_t>(value.size())
    };
}

std::string toSoPath(const std::string& plugin_root) {
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_token_authkey.so";
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
    out_user_uuid[0] = 0x99;
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

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: token_authkey_plugin_selftest <auth_plugin_output_root>\n";
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
    host_api.const_time_equal = &constTimeEqual;
    host_api.now_unix_ms = &nowUnixMs;

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
    std::string username = "token_user";
    conn.username = toSlice(username);
    conn.peer_uid = 1000;
    conn.peer_gid = 1000;
    conn.peer_pid = 111;

    sb_auth_exchange_t exchange = 0;
    sb_auth_step_result_v1 result{};

    // AT-B04-01: valid signed token accepted.
    const std::string good_token = tokenFor("scratchbird.auth.authkey",
                                            "scratchbird",
                                            "sub:token-user",
                                            nowUnixMs() + 60000,
                                            "token-signing-key");
    sb_auth_rc_t rc = api->begin_auth(instance,
                                      toSlice(std::string(kMethodAuthKeyToken)),
                                      &conn,
                                      toSlice(good_token),
                                      &exchange,
                                      &result);
    ok = expect(rc == SB_AUTH_RC_OK, "valid signed token should allow") && ok;

    // H2: extra/unknown fields are rejected as malformed token input.
    std::memset(&result, 0, sizeof(result));
    const std::string malformed_unknown_field_token = good_token + ";role=dba";
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodAuthKeyToken)),
                         &conn,
                         toSlice(malformed_unknown_field_token),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "token with unknown field should deny as malformed") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedToken,
                "unknown field malformed error code mismatch") &&
         ok;

    // AT-B04-02: expired token denied.
    std::memset(&result, 0, sizeof(result));
    const std::string expired_token = tokenFor("scratchbird.auth.authkey",
                                               "scratchbird",
                                               "sub:token-user",
                                               nowUnixMs() - 1000,
                                               "token-signing-key");
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodAuthKeyToken)),
                         &conn,
                         toSlice(expired_token),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "expired token should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrExpired,
                "expired token error code mismatch") &&
         ok;

    // AT-B04-02: wrong audience denied.
    std::memset(&result, 0, sizeof(result));
    const std::string wrong_aud_token = tokenFor("scratchbird.auth.authkey",
                                                 "other-audience",
                                                 "sub:token-user",
                                                 nowUnixMs() + 60000,
                                                 "token-signing-key");
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodAuthKeyToken)),
                         &conn,
                         toSlice(wrong_aud_token),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "wrong audience token should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrAudienceMismatch,
                "wrong audience error code mismatch") &&
         ok;

    ok = expect(g_audit_events >= 3, "expected audit events for token begin calls") && ok;

    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "token_authkey_plugin_selftest: PASS\n";
    return 0;
}
