/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "auth_plugin_secret_provider.h"

#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

namespace {

using scratchbird::security::plugins::secrets::SecretMaterial;
using scratchbird::security::plugins::secrets::SecretResolveStatus;

std::unordered_map<std::string, std::string> g_policy_store;
bool g_fail_policy_reads = false;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

sb_auth_rc_t readPolicyValue(sb_auth_slice_t key, sb_auth_slice_t* out_value) {
    if (!out_value) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    if (g_fail_policy_reads || !key.ptr || key.len == 0) {
        return SB_AUTH_RC_DENY;
    }

    const std::string key_name(reinterpret_cast<const char*>(key.ptr),
                               reinterpret_cast<const char*>(key.ptr) + key.len);
    auto it = g_policy_store.find(key_name);
    if (it == g_policy_store.end()) {
        return SB_AUTH_RC_DENY;
    }

    out_value->ptr = reinterpret_cast<const uint8_t*>(it->second.data());
    out_value->len = static_cast<uint32_t>(it->second.size());
    return SB_AUTH_RC_OK;
}

}  // namespace

int main() {
    bool ok = true;

    sb_auth_host_api_v1 host_api{};
    host_api.struct_size = sizeof(sb_auth_host_api_v1);
    host_api.read_policy_value = &readPolicyValue;

    SecretMaterial material{};
    std::string error;

    auto status = scratchbird::security::plugins::secrets::resolveSecretReference(
        &host_api,
        "literal:inline-secret",
        &material,
        &error);
    ok = expect(status == SecretResolveStatus::OK,
                "literal secret should resolve successfully") &&
         ok;
    ok = expect(material.value == "inline-secret",
                "literal secret should preserve value") &&
         ok;
    ok = expect(material.source == "literal",
                "literal secret should report literal source") &&
         ok;

    material = SecretMaterial{};
    status = scratchbird::security::plugins::secrets::resolveSecretReference(
        &host_api,
        "",
        &material,
        &error);
    ok = expect(status == SecretResolveStatus::MISSING_REFERENCE,
                "empty secret reference should fail closed") &&
         ok;

    g_policy_store["auth.radius.secret_ref"] = "v1";
    material = SecretMaterial{};
    status = scratchbird::security::plugins::secrets::resolveSecretReference(
        &host_api,
        "policy:auth.radius.secret_ref",
        &material,
        &error);
    ok = expect(status == SecretResolveStatus::OK,
                "policy secret should resolve successfully") &&
         ok;
    ok = expect(material.value == "v1", "policy secret should resolve initial value") && ok;
    ok = expect(material.source == "policy", "policy secret should report policy source") && ok;
    ok = expect(material.supports_rotation,
                "policy secret should be marked as rotation-capable") &&
         ok;

    // Simulate rotated key material and ensure fresh reads pick up new value.
    g_policy_store["auth.radius.secret_ref"] = "v2";
    material = SecretMaterial{};
    status = scratchbird::security::plugins::secrets::resolveSecretReference(
        &host_api,
        "policy:auth.radius.secret_ref",
        &material,
        &error);
    ok = expect(status == SecretResolveStatus::OK,
                "policy secret should resolve after rotation") &&
         ok;
    ok = expect(material.value == "v2",
                "policy-backed secret should pick up rotated value without restart") &&
         ok;

    g_fail_policy_reads = true;
    material = SecretMaterial{};
    status = scratchbird::security::plugins::secrets::resolveSecretReference(
        &host_api,
        "policy:auth.radius.secret_ref",
        &material,
        &error);
    ok = expect(status == SecretResolveStatus::POLICY_READ_FAILED,
                "failed policy read should fail closed") &&
         ok;
    g_fail_policy_reads = false;

    if (!ok) {
        return 1;
    }

    std::cout << "auth_plugin_secret_provider_selftest: PASS\n";
    return 0;
}
