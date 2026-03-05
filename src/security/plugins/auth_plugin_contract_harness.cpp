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
    {"auth.password_compat.credential_ref.harness_user", "literal:harness_payload"},
    {"auth.radius.shared_secret_ref", "radius-secret-v1"},
    {"auth.kerberos.keytab_ref", "/etc/security/keytabs/krb5.keytab"},
    {"auth.token_authkey.signing_key_ref", "token-signing-key"},
    {"auth.jwt_oidc.jwt_signing_key_ref", "jwt-signing-key"},
    {"auth.jwt_oidc.oidc_signing_key_ref", "oidc-signing-key"},
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
        out[i] = static_cast<uint8_t>((i * 37u + 19u) & 0xFFu);
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

sb_auth_rc_t resolveUserByExternalSubject(sb_auth_slice_t /*issuer*/,
                                          sb_auth_slice_t subject,
                                          uint8_t out_user_uuid[SB_AUTH_UUID_BYTES]) {
    if (!out_user_uuid || !subject.ptr || subject.len == 0) {
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

bool runCase(const std::string& plugin_root,
             const PluginCase& plugin,
             const sb_auth_host_api_v1& host_api) {
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

    ok = expect(std::string(descriptor->plugin_id) == plugin.plugin_id,
                "descriptor plugin_id mismatch for " + path) &&
         ok;
    ok = expect(descriptor->method_count > 0 && descriptor->methods != nullptr,
                "descriptor missing methods for " + path) &&
         ok;
    if (!ok) {
        dlclose(handle);
        return false;
    }

    sb_auth_plugin_instance_t instance = 0;
    ok = expect(api->create_instance != nullptr, "create_instance missing for " + path) && ok;
    if (!ok) {
        dlclose(handle);
        return false;
    }
    ok = expect(api->create_instance(&instance) == SB_AUTH_RC_OK,
                "create_instance failed for " + path) &&
         ok;

    const std::string cfg_text = plugin.config_json ? plugin.config_json : "";
    const sb_auth_slice_t config_slice = toSlice(cfg_text);
    ok = expect(api->configure_instance(instance, config_slice) == SB_AUTH_RC_OK,
                "configure_instance failed for " + path) &&
         ok;

    sb_auth_slice_t health{};
    ok = expect(api->health_check(instance, &health) == SB_AUTH_RC_OK,
                "health_check failed for " + path) &&
         ok;
    ok = expect(health.ptr != nullptr && health.len > 0, "health payload empty for " + path) &&
         ok;

    std::string username = "harness_user";
    std::string addr = "127.0.0.1";
    sb_auth_connection_ctx_v1 conn{};
    conn.struct_size = sizeof(sb_auth_connection_ctx_v1);
    conn.transport = SB_AUTH_TRANSPORT_LOCAL;
    conn.username = toSlice(username);
    conn.client_address = toSlice(addr);
    conn.server_address = toSlice(addr);
    conn.peer_uid = 1000;
    conn.peer_gid = 1000;
    conn.peer_pid = 4321;

    const std::string method_text = descriptor->methods[0].method_id;
    const sb_auth_slice_t method_slice = toSlice(method_text);
    std::string payload_text = "harness_payload";
    if (std::strcmp(plugin.plugin_dir, "trust_reject") == 0) {
        payload_text = "reason=harness;request_id=contract";
    } else if (std::strcmp(plugin.plugin_dir, "peer") == 0) {
        payload_text.clear();
    }
    sb_auth_slice_t payload_slice = toSlice(payload_text);

    sb_auth_exchange_t exchange = 0;
    sb_auth_step_result_v1 result{};
    const sb_auth_rc_t begin_rc = api->begin_auth(
        instance,
        method_slice,
        &conn,
        payload_slice,
        &exchange,
        &result);
    ok = expect(begin_rc != SB_AUTH_RC_ERROR, "begin_auth returned error for " + path) && ok;

    if (begin_rc == SB_AUTH_RC_CONTINUE) {
        sb_auth_step_result_v1 cont_result{};
        const sb_auth_rc_t cont_rc =
            api->continue_auth(instance, exchange, payload_slice, &cont_result);
        ok = expect(cont_rc != SB_AUTH_RC_ERROR, "continue_auth returned error for " + path) && ok;
    } else if (exchange != 0) {
        api->abort_auth(instance, exchange);
    }

    api->destroy_instance(instance);
    dlclose(handle);
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: auth_plugin_contract_harness <auth_plugin_output_root>\n";
        return 2;
    }

    const std::string plugin_root = argv[1];

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
        ok = runCase(plugin_root, plugin, host_api) && ok;
    }

    if (!ok) {
        return 1;
    }

    std::cout << "auth_plugin_contract_harness: PASS\n";
    return 0;
}
