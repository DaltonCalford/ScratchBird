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

constexpr char kPluginId[] = "scratchbird.auth.factor_chain";
constexpr char kMethodFactorChain2fa[] = "scratchbird.auth.factor_chain_2fa";
constexpr char kPolicySigningKeyRef[] = "auth.factor_chain.signing_key_ref";
constexpr char kPolicySequence2fa[] = "auth.factor_chain.2fa.sequence";
constexpr char kPolicyRequireDistinct[] = "auth.factor_chain.require_distinct_factors";
constexpr char kErrFactorOrder[] = "AUTH_FACTOR_CHAIN_FACTOR_ORDER";

std::unordered_map<std::string, std::string> g_policy_store{
    {kPolicySigningKeyRef, "factor-chain-key-v1"},
    {kPolicySequence2fa, "password,otp"},
    {kPolicyRequireDistinct, "true"},
};
uint64_t g_audit_events = 0;

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

std::string factorProof(const std::string& method,
                        const std::string& subject,
                        const std::string& factor,
                        uint8_t stage,
                        const std::string& nonce,
                        const std::string& signing_key) {
    const std::string payload = method + "|" + subject + "|" + factor + "|" +
                                std::to_string(static_cast<unsigned>(stage)) + "|" + nonce;
    std::string proof;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(signing_key, payload, &proof)) {
        return "";
    }
    return proof;
}

sb_auth_slice_t toSlice(const std::string& value) {
    return sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(value.data()),
        static_cast<uint32_t>(value.size())
    };
}

std::string toSoPath(const std::string& plugin_root) {
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_factor_chain.so";
}

std::string sliceToString(sb_auth_slice_t value) {
    if (!value.ptr || value.len == 0) {
        return "";
    }
    const char* begin = reinterpret_cast<const char*>(value.ptr);
    return std::string(begin, begin + value.len);
}

std::string keyValueField(const std::string& payload, const std::string& key) {
    std::size_t cursor = 0;
    while (cursor < payload.size()) {
        const std::size_t sep = payload.find(';', cursor);
        const std::size_t end = (sep == std::string::npos) ? payload.size() : sep;
        if (end <= cursor) {
            break;
        }

        const std::string token = payload.substr(cursor, end - cursor);
        const std::size_t eq = token.find('=');
        if (eq != std::string::npos && eq > 0 && eq + 1 < token.size()) {
            const std::string token_key = token.substr(0, eq);
            if (token_key == key) {
                return token.substr(eq + 1);
            }
        }

        if (sep == std::string::npos) {
            break;
        }
        cursor = sep + 1;
    }
    return "";
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
    const std::string issuer_text(reinterpret_cast<const char*>(issuer.ptr),
                                  reinterpret_cast<const char*>(issuer.ptr) + issuer.len);
    if (issuer_text != "factor_chain") {
        return SB_AUTH_RC_DENY;
    }
    std::memset(out_user_uuid, 0, SB_AUTH_UUID_BYTES);
    out_user_uuid[0] = 0xE5;
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
        std::cerr << "usage: factor_chain_plugin_selftest <auth_plugin_output_root>\n";
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

    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_LOCAL;
    std::string username = "chain_user";
    conn.username = toSlice(username);
    conn.peer_uid = 1000;
    conn.peer_gid = 1000;
    conn.peer_pid = 5555;

    const std::string subject = "sub:chain-user";
    const std::string begin_payload = "user=chain_user;sub=" + subject;
    sb_auth_exchange_t exchange = 0;
    sb_auth_step_result_v1 result{};

    // AT-C03-01: required chain completion accepted.
    sb_auth_rc_t rc = api->begin_auth(instance,
                                      toSlice(std::string(kMethodFactorChain2fa)),
                                      &conn,
                                      toSlice(begin_payload),
                                      &exchange,
                                      &result);
    ok = expect(rc == SB_AUTH_RC_CONTINUE, "2FA chain begin should continue") && ok;
    ok = expect(exchange != 0, "begin should produce exchange") && ok;

    const std::string step1_payload = sliceToString(result.payload);
    const std::string nonce1 = keyValueField(step1_payload, "nonce");
    const std::string proof1 = factorProof(kMethodFactorChain2fa,
                                           subject,
                                           "password",
                                           1,
                                           nonce1,
                                           "factor-chain-key-v1");
    const std::string factor1 =
        "factor=password;status=ok;sub=" + subject + ";nonce=" + nonce1 + ";proof=" + proof1;

    std::memset(&result, 0, sizeof(result));
    rc = api->continue_auth(instance, exchange, toSlice(factor1), &result);
    ok = expect(rc == SB_AUTH_RC_CONTINUE, "first factor should continue chain") && ok;

    const std::string step2_payload = sliceToString(result.payload);
    const std::string nonce2 = keyValueField(step2_payload, "nonce");
    const std::string proof2 = factorProof(kMethodFactorChain2fa,
                                           subject,
                                           "otp",
                                           2,
                                           nonce2,
                                           "factor-chain-key-v1");
    const std::string factor2 =
        "factor=otp;status=ok;sub=" + subject + ";nonce=" + nonce2 + ";proof=" + proof2;

    std::memset(&result, 0, sizeof(result));
    rc = api->continue_auth(instance, exchange, toSlice(factor2), &result);
    ok = expect(rc == SB_AUTH_RC_OK, "second factor should complete chain") && ok;

    // AT-C03-02: skipped factor denied.
    exchange = 0;
    std::memset(&result, 0, sizeof(result));
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodFactorChain2fa)),
                         &conn,
                         toSlice(begin_payload),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_CONTINUE, "second 2FA begin should continue") && ok;

    const std::string bad_step_payload = sliceToString(result.payload);
    const std::string bad_nonce = keyValueField(bad_step_payload, "nonce");
    const std::string bad_proof = factorProof(kMethodFactorChain2fa,
                                              subject,
                                              "otp",
                                              1,
                                              bad_nonce,
                                              "factor-chain-key-v1");
    const std::string skipped_factor =
        "factor=otp;status=ok;sub=" + subject + ";nonce=" + bad_nonce + ";proof=" + bad_proof;

    std::memset(&result, 0, sizeof(result));
    rc = api->continue_auth(instance, exchange, toSlice(skipped_factor), &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "skipped/incorrect factor order should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrFactorOrder,
                "factor order error code mismatch") &&
         ok;

    ok = expect(g_audit_events >= 5, "expected audit events for factor-chain flow") && ok;

    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "factor_chain_plugin_selftest: PASS\n";
    return 0;
}
