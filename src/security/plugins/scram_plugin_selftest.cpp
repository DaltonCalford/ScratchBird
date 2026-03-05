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

constexpr char kPluginId[] = "scratchbird.auth.scram";
constexpr char kMethodScramSha256[] = "scratchbird.auth.scram_sha_256";
constexpr char kPolicyCredentialRefUser[] = "auth.scram.credential_ref.scram_user";
constexpr char kErrProofInvalid[] = "AUTH_SCRAM_PROOF_INVALID";
constexpr char kErrReplayDetected[] = "AUTH_SCRAM_REPLAY_DETECTED";

std::unordered_map<std::string, std::string> g_policy_store{
    {kPolicyCredentialRefUser, "literal:salt=sodium;secret=scram-secret;iter=4096"},
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
    return plugin_root + "/" + kPluginId + "/libscratchbird_auth_scram.so";
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

sb_auth_rc_t resolveUserByName(sb_auth_slice_t username,
                               uint8_t out_user_uuid[SB_AUTH_UUID_BYTES]) {
    if (!username.ptr || username.len == 0 || !out_user_uuid) {
        return SB_AUTH_RC_DENY;
    }
    const std::string user_text(reinterpret_cast<const char*>(username.ptr),
                                reinterpret_cast<const char*>(username.ptr) + username.len);
    if (user_text != "scram_user") {
        return SB_AUTH_RC_DENY;
    }
    std::memset(out_user_uuid, 0, SB_AUTH_UUID_BYTES);
    out_user_uuid[0] = 0xC1;
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
        std::cerr << "usage: scram_plugin_selftest <auth_plugin_output_root>\n";
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

    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_LOCAL;
    std::string username = "scram_user";
    conn.username = toSlice(username);
    conn.peer_uid = 1000;
    conn.peer_gid = 1000;
    conn.peer_pid = 31337;

    // AT-C01-01: valid client proof accepted.
    const std::string client_nonce = "cn-1";
    const std::string begin_payload =
        "user=scram_user;cnonce=" + client_nonce;
    sb_auth_exchange_t exchange = 0;
    sb_auth_step_result_v1 result{};
    sb_auth_rc_t rc = api->begin_auth(instance,
                                      toSlice(std::string(kMethodScramSha256)),
                                      &conn,
                                      toSlice(begin_payload),
                                      &exchange,
                                      &result);
    ok = expect(rc == SB_AUTH_RC_CONTINUE, "valid SCRAM first message should continue") && ok;
    ok = expect(exchange != 0, "begin_auth should return non-zero exchange") && ok;

    const std::string challenge_payload = sliceToString(result.payload);
    const std::string server_nonce = keyValueField(challenge_payload, "snonce");
    const std::string combined_nonce = keyValueField(challenge_payload, "nonce");
    ok = expect(!server_nonce.empty(), "challenge missing server nonce") && ok;
    ok = expect(combined_nonce == client_nonce + server_nonce,
                "combined nonce mismatch in challenge") &&
         ok;

    const std::string proof_input =
        std::string(kMethodScramSha256) + "|scram_user|" + client_nonce + "|" + server_nonce +
        "|sodium|4096";
    std::string proof;
    ok = expect(scratchbird::security::plugins::crypto::hmacSha256Hex("scram-secret",
                                                                       proof_input,
                                                                       &proof),
                "failed to build SCRAM proof") &&
         ok;
    const std::string continue_payload = "proof=" + proof + ";nonce=" + combined_nonce;

    std::memset(&result, 0, sizeof(result));
    rc = api->continue_auth(instance, exchange, toSlice(continue_payload), &result);
    ok = expect(rc == SB_AUTH_RC_OK, "valid SCRAM proof should allow") && ok;

    // AT-C01-02: replay denied.
    std::memset(&result, 0, sizeof(result));
    rc = api->continue_auth(instance, exchange, toSlice(continue_payload), &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "replayed SCRAM exchange should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrReplayDetected,
                "replay error code mismatch") &&
         ok;

    // AT-C01-02: invalid proof denied.
    const std::string client_nonce2 = "cn-2";
    const std::string begin_payload2 =
        "user=scram_user;cnonce=" + client_nonce2;
    exchange = 0;
    std::memset(&result, 0, sizeof(result));
    rc = api->begin_auth(instance,
                         toSlice(std::string(kMethodScramSha256)),
                         &conn,
                         toSlice(begin_payload2),
                         &exchange,
                         &result);
    ok = expect(rc == SB_AUTH_RC_CONTINUE, "second SCRAM begin should continue") && ok;

    const std::string challenge_payload2 = sliceToString(result.payload);
    const std::string combined_nonce2 = keyValueField(challenge_payload2, "nonce");
    const std::string bad_proof_payload = "proof=0000000000000000;nonce=" + combined_nonce2;
    std::memset(&result, 0, sizeof(result));
    rc = api->continue_auth(instance, exchange, toSlice(bad_proof_payload), &result);
    ok = expect(rc == SB_AUTH_RC_DENY, "invalid SCRAM proof should deny") && ok;
    ok = expect(std::string(result.plugin_error_code) == kErrProofInvalid,
                "invalid proof error code mismatch") &&
         ok;

    ok = expect(g_audit_events >= 5, "expected audit events for SCRAM flow") && ok;

    api->destroy_instance(instance);
    dlclose(handle);
    if (!ok) {
        return 1;
    }

    std::cout << "scram_plugin_selftest: PASS\n";
    return 0;
}
