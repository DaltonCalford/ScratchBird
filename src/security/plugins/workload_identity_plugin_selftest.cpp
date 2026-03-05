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

constexpr char kPluginId[] = "scratchbird.auth.workload_identity";
constexpr char kMethodWorkloadOidc[] = "scratchbird.auth.workload_oidc";
constexpr char kMethodWorkloadSpiffe[] = "scratchbird.auth.workload_spiffe";

constexpr char kPolicyOidcSigningKeyRef[] = "auth.workload_identity.oidc_signing_key_ref";
constexpr char kPolicySpiffeSigningKeyRef[] = "auth.workload_identity.spiffe_signing_key_ref";
constexpr char kPolicyOidcExpectedIssuer[] = "auth.workload_identity.oidc_expected_issuer";
constexpr char kPolicyOidcExpectedAudience[] = "auth.workload_identity.oidc_expected_audience";
constexpr char kPolicyOidcTrustBundle[] = "auth.workload_identity.oidc_trust_bundle";
constexpr char kPolicyOidcRequiredAlg[] = "auth.workload_identity.oidc_required_alg";
constexpr char kPolicyOidcSubjectPrefix[] = "auth.workload_identity.oidc_subject_prefix";
constexpr char kPolicySpiffeTrustBundle[] = "auth.workload_identity.spiffe_trust_bundle";
constexpr char kPolicySpiffeSubjectPrefix[] = "auth.workload_identity.spiffe_subject_prefix";

constexpr char kErrMalformedSubject[] = "AUTH_WORKLOAD_SUBJECT_MALFORMED";
constexpr char kErrBundleInvalid[] = "AUTH_WORKLOAD_TRUST_BUNDLE_INVALID";
constexpr char kErrSubjectPolicyMismatch[] = "AUTH_WORKLOAD_SUBJECT_POLICY_MISMATCH";

std::unordered_map<std::string, std::string> g_policy_store{
    {kPolicyOidcSigningKeyRef, "workload-oidc-key-v1"},
    {kPolicySpiffeSigningKeyRef, "workload-spiffe-key-v1"},
    {kPolicyOidcExpectedIssuer, "workload-issuer"},
    {kPolicyOidcExpectedAudience, "scratchbird-workload"},
    {kPolicyOidcTrustBundle, "oidc-bundle-v1"},
    {kPolicyOidcRequiredAlg, "HS256"},
    {kPolicyOidcSubjectPrefix, "svc:"},
    {kPolicySpiffeTrustBundle, "spiffe-bundle-v1"},
    {kPolicySpiffeSubjectPrefix, "spiffe://prod/"},
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
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_workload_identity.so";
}

uint64_t nowUnixMs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string base64UrlEncode(const std::string& input) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    std::size_t i = 0;
    while (i + 3 <= input.size()) {
        const uint32_t chunk =
            (static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16u) |
            (static_cast<uint32_t>(static_cast<unsigned char>(input[i + 1])) << 8u) |
            static_cast<uint32_t>(static_cast<unsigned char>(input[i + 2]));
        out.push_back(kAlphabet[(chunk >> 18u) & 0x3Fu]);
        out.push_back(kAlphabet[(chunk >> 12u) & 0x3Fu]);
        out.push_back(kAlphabet[(chunk >> 6u) & 0x3Fu]);
        out.push_back(kAlphabet[chunk & 0x3Fu]);
        i += 3;
    }

    const std::size_t rem = input.size() - i;
    if (rem == 1) {
        const uint32_t chunk =
            (static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16u);
        out.push_back(kAlphabet[(chunk >> 18u) & 0x3Fu]);
        out.push_back(kAlphabet[(chunk >> 12u) & 0x3Fu]);
    } else if (rem == 2) {
        const uint32_t chunk =
            (static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16u) |
            (static_cast<uint32_t>(static_cast<unsigned char>(input[i + 1])) << 8u);
        out.push_back(kAlphabet[(chunk >> 18u) & 0x3Fu]);
        out.push_back(kAlphabet[(chunk >> 12u) & 0x3Fu]);
        out.push_back(kAlphabet[(chunk >> 6u) & 0x3Fu]);
    }

    return out;
}

std::string makeOidcToken(const std::string& issuer,
                          const std::string& audience,
                          const std::string& subject,
                          const std::string& bundle,
                          uint64_t exp_unix_ms,
                          const std::string& signing_key,
                          const std::string& alg = "HS256") {
    const std::string header_json = "{\"alg\":\"" + alg + "\",\"typ\":\"JWT\"}";
    const std::string payload_json =
        "{\"iss\":\"" + issuer + "\",\"aud\":\"" + audience + "\",\"sub\":\"" + subject +
        "\",\"bundle\":\"" + bundle + "\",\"exp\":" + std::to_string(exp_unix_ms) + "}";

    const std::string header_segment = base64UrlEncode(header_json);
    const std::string payload_segment = base64UrlEncode(payload_json);
    std::string signature;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(signing_key,
                                                                header_segment + "." +
                                                                    payload_segment,
                                                                &signature)) {
        return "";
    }
    return header_segment + "." + payload_segment + "." + signature;
}

std::string makeSpiffeAssertion(const std::string& spiffe_id,
                                const std::string& bundle,
                                uint64_t exp_unix_ms,
                                const std::string& signing_key) {
    const std::string sig_payload =
        spiffe_id + "|" + bundle + "|" + std::to_string(exp_unix_ms);
    std::string sig;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(signing_key, sig_payload, &sig)) {
        return "";
    }
    return "spiffe=" + spiffe_id + ";bundle=" + bundle + ";exp=" +
           std::to_string(exp_unix_ms) + ";sig=" + sig;
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
    out_user_uuid[0] = 0xD1;
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
        std::cerr << "usage: workload_identity_plugin_selftest <auth_plugin_output_root>\n";
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
    std::string username = "workload_user";
    conn.username = toSlice(username);
    conn.peer_uid = 1000;
    conn.peer_gid = 1000;
    conn.peer_pid = 321;

    sb_auth_exchange_t exchange = 0;
    sb_auth_step_result_v1 result{};

    // AT-B09-01: valid workload identities accepted.
    const std::string valid_oidc = makeOidcToken(
        "workload-issuer",
        "scratchbird-workload",
        "svc:payments",
        "oidc-bundle-v1",
        nowUnixMs() + 60000,
        "workload-oidc-key-v1");
    sb_auth_rc_t rc = api->begin_auth(instance,
                                      toSlice(std::string(kMethodWorkloadOidc)),
                                      &conn,
                                      toSlice(valid_oidc),
                                      &exchange,
                                      &result);
    ok = expect(rc == SB_AUTH_RC_OK, "valid workload oidc token should allow") && ok;

    std::memset(&result, 0, sizeof(result));
    const std::string valid_spiffe = makeSpiffeAssertion(
        "spiffe://prod/ns/default/sa/payments",
        "spiffe-bundle-v1",
        nowUnixMs() + 60000,
        "workload-spiffe-key-v1");
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodWorkloadSpiffe)),
                         &conn,
                         toSlice(valid_spiffe),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_OK, "valid spiffe workload assertion should allow") && ok;

    // H2: unknown SPIFFE assertion fields are malformed input.
    std::memset(&result, 0, sizeof(result));
    const std::string malformed_spiffe = valid_spiffe + ";tenant=blue";
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodWorkloadSpiffe)),
                         &conn,
                         toSlice(malformed_spiffe),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "spiffe assertion with unknown field should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrMalformedSubject,
                "unknown SPIFFE field malformed error code mismatch") &&
         ok;

    // AT-B09-02: invalid bundle denied.
    std::memset(&result, 0, sizeof(result));
    const std::string wrong_bundle_oidc = makeOidcToken(
        "workload-issuer",
        "scratchbird-workload",
        "svc:payments",
        "oidc-bundle-evil",
        nowUnixMs() + 60000,
        "workload-oidc-key-v1");
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodWorkloadOidc)),
                         &conn,
                         toSlice(wrong_bundle_oidc),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "oidc token with invalid bundle should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrBundleInvalid,
                "invalid bundle error code mismatch") &&
         ok;

    // AT-B09-02: invalid subject denied.
    std::memset(&result, 0, sizeof(result));
    const std::string bad_subject_spiffe = makeSpiffeAssertion(
        "spiffe://dev/ns/default/sa/payments",
        "spiffe-bundle-v1",
        nowUnixMs() + 60000,
        "workload-spiffe-key-v1");
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodWorkloadSpiffe)),
                         &conn,
                         toSlice(bad_subject_spiffe),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "spiffe subject outside allowed prefix should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrSubjectPolicyMismatch,
                "subject policy mismatch error code mismatch") &&
         ok;

    // Rotation check: trust bundle updates should be observed without restart.
    std::memset(&result, 0, sizeof(result));
    g_policy_store[kPolicyOidcTrustBundle] = "oidc-bundle-v2";
    const std::string rotated_bundle_token = makeOidcToken(
        "workload-issuer",
        "scratchbird-workload",
        "svc:payments",
        "oidc-bundle-v2",
        nowUnixMs() + 60000,
        "workload-oidc-key-v1");
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodWorkloadOidc)),
                         &conn,
                         toSlice(rotated_bundle_token),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_OK, "rotated trust bundle should be honored") && ok;

    ok = expect(g_audit_events >= 6, "expected audit events for workload begin calls") && ok;

    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "workload_identity_plugin_selftest: PASS\n";
    return 0;
}
