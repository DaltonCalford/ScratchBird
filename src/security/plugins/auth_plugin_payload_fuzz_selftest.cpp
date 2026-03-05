/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "scratchbird/security/auth_plugin_abi_v1.h"

#include <dlfcn.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct PluginCase {
    const char* plugin_dir;
    const char* plugin_id;
    const char* config_json;
};

const std::array<PluginCase, 17> kPluginCases{{
    {"trust_reject", "scratchbird.auth.trust_reject", ""},
    {"password_compat", "scratchbird.auth.password_compat", ""},
    {"scram", "scratchbird.auth.scram", ""},
    {"token_authkey", "scratchbird.auth.token_authkey", ""},
    {"peer", "scratchbird.auth.peer", ""},
    {"certificate_mtls", "scratchbird.auth.certificate_mtls", ""},
    {"jwt_oidc", "scratchbird.auth.jwt_oidc", ""},
    {"webauthn", "scratchbird.auth.webauthn", ""},
    {"factor_chain", "scratchbird.auth.factor_chain", ""},
    {"workload_identity", "scratchbird.auth.workload_identity", ""},
    {"oauth_validator", "scratchbird.auth.oauth_validator", ""},
    {"proxy_assertion", "scratchbird.auth.proxy_assertion", ""},
    {"ldap",
     "scratchbird.auth.ldap",
     "{\"ldap_uri\":\"ldaps://ldap.example\",\"bind_dn_template\":\"uid={user},ou=People,dc=example,dc=com\",\"group_role_map\":\"cn=dba:DBA\",\"require_starttls\":false,\"allowed_ldap_endpoints\":[\"127.0.0.1\"]}"},
    {"kerberos",
     "scratchbird.auth.kerberos",
     "{\"service_principal\":\"postgres/host@example.com\",\"keytab_path\":\"policy:auth.kerberos.keytab_ref\",\"allowed_kdc_endpoints\":[\"127.0.0.1\"]}"},
    {"ident",
     "scratchbird.auth.ident",
     "{\"trusted_cidrs\":[\"127.0.0.1/32\"],\"ident_timeout_ms\":1000,\"require_username_match\":false}"},
    {"radius",
     "scratchbird.auth.radius",
     "{\"radius_servers\":[\"127.0.0.1\"],\"shared_secret_ref\":\"auth.radius.shared_secret_ref\",\"request_timeout_ms\":1000,\"allowed_radius_endpoints\":[\"127.0.0.1\"]}"},
    {"pam",
     "scratchbird.auth.pam",
     "{\"service_name\":\"login\",\"allowed_modules\":[\"pam_unix.so\"],\"conversation_timeout_ms\":1000}"},
}};

std::unordered_map<std::string, std::string> g_policy_store{
    {"auth.password_compat.mode", "bootstrap_allow"},
    {"auth.password_compat.allow_md5_legacy", "false"},
    {"auth.password_compat.credential_ref.fuzz_user", "literal:fuzz-secret"},
    {"auth.scram.default_credential_ref", "literal:salt=fuzz;secret=fuzz-secret;iter=4096"},
    {"auth.radius.shared_secret_ref", "radius-secret-v1"},
    {"auth.kerberos.keytab_ref", "krb5-keytab-v1"},
    {"auth.token_authkey.signing_key_ref", "token-signing-key"},
    {"auth.jwt_oidc.jwt_signing_key_ref", "jwt-signing-key"},
    {"auth.jwt_oidc.oidc_signing_key_ref", "oidc-signing-key"},
    {"auth.oauth_validator.signing_key_ref", "oauth-signing-key"},
    {"auth.proxy_assertion.signing_key_ref", "proxy-signing-key"},
    {"auth.workload_identity.oidc_signing_key_ref", "oidc-workload-signing-key"},
    {"auth.workload_identity.spiffe_signing_key_ref", "spiffe-workload-signing-key"},
    {"auth.webauthn.signing_key_ref", "webauthn-signing-key"},
    {"auth.factor_chain.signing_key_ref", "factor-chain-signing-key"},
};

uint64_t nowUnixMs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

sb_auth_rc_t secureRandom(uint8_t* out, uint32_t len) {
    if (!out) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    for (uint32_t i = 0; i < len; ++i) {
        out[i] = static_cast<uint8_t>((i * 13u + 97u) & 0xFFu);
    }
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

sb_auth_rc_t resolveUserByName(sb_auth_slice_t username,
                               uint8_t out_user_uuid[SB_AUTH_UUID_BYTES]) {
    if (!out_user_uuid || !username.ptr || username.len == 0) {
        return SB_AUTH_RC_DENY;
    }
    std::memset(out_user_uuid, 0, SB_AUTH_UUID_BYTES);
    out_user_uuid[0] = 0xA1;
    return SB_AUTH_RC_OK;
}

sb_auth_rc_t resolveUserByExternalSubject(sb_auth_slice_t issuer,
                                          sb_auth_slice_t subject,
                                          uint8_t out_user_uuid[SB_AUTH_UUID_BYTES]) {
    if (!out_user_uuid || !subject.ptr || subject.len == 0 || !issuer.ptr || issuer.len == 0) {
        return SB_AUTH_RC_DENY;
    }
    std::memset(out_user_uuid, 0, SB_AUTH_UUID_BYTES);
    out_user_uuid[0] = 0xB2;
    return SB_AUTH_RC_OK;
}

sb_auth_rc_t emitAuditEvent(sb_auth_slice_t /*event_name*/, sb_auth_slice_t /*event_json*/) {
    return SB_AUTH_RC_OK;
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

void* hostAlloc(uint32_t size) {
    return std::malloc(size);
}

void hostDealloc(void* ptr) {
    std::free(ptr);
}

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

bool isKnownRc(sb_auth_rc_t rc) {
    switch (rc) {
        case SB_AUTH_RC_OK:
        case SB_AUTH_RC_CONTINUE:
        case SB_AUTH_RC_DENY:
        case SB_AUTH_RC_ERROR:
        case SB_AUTH_RC_UNSUPPORTED:
        case SB_AUTH_RC_INVALID_ARGUMENT:
        case SB_AUTH_RC_POLICY_VIOLATION:
        case SB_AUTH_RC_SIGNATURE_INVALID:
        case SB_AUTH_RC_UNAUTHORIZED_PLUGIN:
            return true;
        default:
            return false;
    }
}

std::string soPath(const std::string& plugin_root, const PluginCase& plugin) {
    return plugin_root + "/" + plugin.plugin_id + "/libscratchbird_auth_" +
           plugin.plugin_dir + ".so";
}

sb_auth_slice_t toSlice(const std::string& value) {
    return sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(value.data()),
        static_cast<uint32_t>(value.size())
    };
}

sb_auth_slice_t toSlice(const std::vector<uint8_t>& value) {
    return sb_auth_slice_t{
        value.empty() ? nullptr : value.data(),
        static_cast<uint32_t>(value.size())
    };
}

std::vector<uint8_t> randomBytes(std::mt19937_64& rng, std::size_t min_len, std::size_t max_len) {
    const std::size_t len =
        std::uniform_int_distribution<std::size_t>(min_len, max_len)(rng);
    std::vector<uint8_t> out(len);
    for (std::size_t i = 0; i < len; ++i) {
        out[i] = static_cast<uint8_t>(std::uniform_int_distribution<int>(0, 255)(rng));
    }
    return out;
}

bool runCase(const std::string& plugin_root,
             const PluginCase& plugin,
             const sb_auth_host_api_v1& host_api,
             std::mt19937_64& rng) {
    const std::string path = soPath(plugin_root, plugin);
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    const char* dl_error = dlerror();
    if (!expect(handle != nullptr,
                "dlopen failed for " + path + ": " +
                    std::string(dl_error ? dl_error : "unknown"))) {
        return false;
    }

    auto* get_api = reinterpret_cast<sb_auth_plugin_get_api_v1_fn>(
        dlsym(handle, "sb_auth_plugin_get_api_v1"));
    if (!expect(get_api != nullptr, "missing sb_auth_plugin_get_api_v1 for " + path)) {
        dlclose(handle);
        return false;
    }

    const sb_auth_plugin_descriptor_v1* descriptor = nullptr;
    const sb_auth_plugin_api_v1* api = nullptr;
    const sb_auth_rc_t get_rc =
        get_api(SB_AUTH_ABI_MAJOR, &host_api, &descriptor, &api);
    bool ok = expect(get_rc == SB_AUTH_RC_OK, "get_api failed for " + path);
    ok = expect(descriptor != nullptr && api != nullptr, "null descriptor/api for " + path) && ok;
    if (!ok) {
        dlclose(handle);
        return false;
    }

    sb_auth_plugin_instance_t instance = 0;
    ok = expect(api->create_instance(&instance) == SB_AUTH_RC_OK,
                "create_instance failed for " + path) &&
         ok;

    const std::string cfg_text = plugin.config_json ? plugin.config_json : "";
    ok = expect(isKnownRc(api->configure_instance(instance, toSlice(cfg_text))),
                "baseline configure rc unknown for " + path) &&
         ok;

    std::string username = "fuzz_user";
    std::string client_addr = "127.0.0.1";
    std::string server_addr = "127.0.0.1";
    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_LOCAL;
    conn.username = toSlice(username);
    conn.client_address = toSlice(client_addr);
    conn.server_address = toSlice(server_addr);
    conn.peer_uid = 1000;
    conn.peer_gid = 1000;
    conn.peer_pid = 4321;

    constexpr std::size_t kIterations = 128;
    for (std::size_t i = 0; i < kIterations; ++i) {
        if (std::uniform_int_distribution<int>(0, 9)(rng) == 0) {
            const auto cfg_fuzz = randomBytes(rng, 0, 256);
            const sb_auth_rc_t cfg_rc = api->configure_instance(instance, toSlice(cfg_fuzz));
            ok = expect(isKnownRc(cfg_rc), "fuzz configure rc unknown for " + path) && ok;
        }

        conn.transport = (std::uniform_int_distribution<int>(0, 1)(rng) == 0)
                             ? SB_AUTH_TRANSPORT_LOCAL
                             : SB_AUTH_TRANSPORT_INET;
        conn.peer_uid = std::uniform_int_distribution<uint32_t>(0, 2000)(rng);
        conn.peer_gid = std::uniform_int_distribution<uint32_t>(0, 2000)(rng);
        conn.peer_pid = std::uniform_int_distribution<uint32_t>(0, 50000)(rng);

        std::vector<uint8_t> method_bytes;
        if (std::uniform_int_distribution<int>(0, 9)(rng) < 7 && descriptor->method_count > 0) {
            const std::string method = descriptor->methods[0].method_id;
            method_bytes.assign(method.begin(), method.end());
        } else {
            method_bytes = randomBytes(rng, 1, 48);
        }

        auto payload = randomBytes(rng, 0, 512);
        sb_auth_exchange_t exchange = 0;
        sb_auth_step_result_v1 result{};
        const sb_auth_rc_t begin_rc = api->begin_auth(instance,
                                                      toSlice(method_bytes),
                                                      &conn,
                                                      toSlice(payload),
                                                      &exchange,
                                                      &result);
        ok = expect(isKnownRc(begin_rc), "fuzz begin rc unknown for " + path) && ok;

        if (begin_rc == SB_AUTH_RC_CONTINUE) {
            auto next_payload = randomBytes(rng, 0, 512);
            sb_auth_step_result_v1 cont_result{};
            const sb_auth_rc_t cont_rc = api->continue_auth(instance,
                                                            exchange,
                                                            toSlice(next_payload),
                                                            &cont_result);
            ok = expect(isKnownRc(cont_rc), "fuzz continue rc unknown for " + path) && ok;
            if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) {
                sb_auth_step_result_v1 replay_result{};
                const sb_auth_rc_t replay_rc = api->continue_auth(instance,
                                                                  exchange,
                                                                  toSlice(next_payload),
                                                                  &replay_result);
                ok = expect(isKnownRc(replay_rc), "fuzz replay rc unknown for " + path) && ok;
            }
        } else if (exchange != 0) {
            api->abort_auth(instance, exchange);
        }

        if (std::uniform_int_distribution<int>(0, 7)(rng) == 0) {
            sb_auth_slice_t health{};
            const sb_auth_rc_t health_rc = api->health_check(instance, &health);
            ok = expect(health_rc == SB_AUTH_RC_OK, "health_check failed in fuzz run for " + path) &&
                 ok;
        }
    }

    api->destroy_instance(instance);
    dlclose(handle);
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: auth_plugin_payload_fuzz_selftest <auth_plugin_output_root>\n";
        return 2;
    }

    const std::string plugin_root = argv[1];
    std::mt19937_64 rng(0xD02F00D5ULL);

    sb_auth_host_api_v1 host_api{};
    host_api.struct_size = sizeof(sb_auth_host_api_v1);
    host_api.now_unix_ms = &nowUnixMs;
    host_api.secure_random = &secureRandom;
    host_api.const_time_equal = &constTimeEqual;
    host_api.resolve_user_by_name = &resolveUserByName;
    host_api.resolve_user_by_external_subject = &resolveUserByExternalSubject;
    host_api.emit_audit_event = &emitAuditEvent;
    host_api.read_policy_value = &readPolicyValue;
    host_api.alloc = &hostAlloc;
    host_api.dealloc = &hostDealloc;

    bool ok = true;
    for (const auto& plugin : kPluginCases) {
        ok = runCase(plugin_root, plugin, host_api, rng) && ok;
    }

    if (!ok) {
        return 1;
    }

    std::cout << "auth_plugin_payload_fuzz_selftest: PASS\n";
    return 0;
}
