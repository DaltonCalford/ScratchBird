/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * @file auth_plugin_manager.cpp
 * @brief Authentication plugin admission and registry implementation.
 */

#include "scratchbird/security/auth_plugin_manager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace scratchbird {
namespace security {

namespace {

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

constexpr const char* kDefaultTrustStorePath = "/etc/scratchbird/auth_plugin_truststore.jwks.json";
constexpr const char* kDefaultPolicyPath = "/etc/scratchbird/auth_plugins.policy.json";
constexpr const char* kPluginSymbolGetApi = "sb_auth_plugin_get_api_v1";

struct BuiltinPluginTemplate {
    const char* plugin_id;
    std::array<const char*, 4> methods;
    std::size_t method_count;
};

const std::array<BuiltinPluginTemplate, 17> kBuiltinPhase1Plugins = {{
    {"scratchbird.auth.trust_reject",
     {"scratchbird.auth.trust", "scratchbird.auth.reject", nullptr, nullptr},
     2},
    {"scratchbird.auth.password_compat",
     {"scratchbird.auth.password_compat", "scratchbird.auth.md5_legacy", nullptr, nullptr},
     2},
    {"scratchbird.auth.scram",
     {"scratchbird.auth.scram_sha_256", "scratchbird.auth.scram_sha_512", nullptr, nullptr},
     2},
    {"scratchbird.auth.token_authkey",
     {"scratchbird.auth.authkey_token", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.peer",
     {"scratchbird.auth.peer_uid", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.certificate_mtls",
     {"scratchbird.auth.certificate_x509", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.jwt_oidc",
     {"scratchbird.auth.jwt_bearer", "scratchbird.auth.oidc_id_token", nullptr, nullptr},
     2},
    {"scratchbird.auth.webauthn",
     {"scratchbird.auth.webauthn_assertion", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.factor_chain",
     {"scratchbird.auth.factor_chain_2fa", "scratchbird.auth.factor_chain_3fa", nullptr, nullptr},
     2},
    {"scratchbird.auth.workload_identity",
     {"scratchbird.auth.workload_oidc", "scratchbird.auth.workload_spiffe", nullptr, nullptr},
     2},
    {"scratchbird.auth.oauth_validator",
     {"scratchbird.auth.oauth_bearer_validated", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.proxy_assertion",
     {"scratchbird.auth.proxy_principal_assertion", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.ldap",
     {"scratchbird.auth.ldap_bind", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.kerberos",
     {"scratchbird.auth.kerberos_gssapi", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.ident",
     {"scratchbird.auth.ident_rfc1413", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.radius",
     {"scratchbird.auth.radius_pap", nullptr, nullptr, nullptr},
     1},
    {"scratchbird.auth.pam",
     {"scratchbird.auth.pam_conversation", nullptr, nullptr, nullptr},
     1},
}};

std::string trimAscii(std::string value) {
    auto is_space = [](unsigned char c) {
        return std::isspace(c) != 0;
    };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool readTextFile(const std::filesystem::path& path, std::string& out_text) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!in.good() && !in.eof()) {
        return false;
    }
    out_text = buffer.str();
    return true;
}

ordered_json canonicalizeJson(const json& value) {
    if (value.is_object()) {
        ordered_json out = ordered_json::object();
        std::vector<std::string> keys;
        keys.reserve(value.size());
        for (auto it = value.begin(); it != value.end(); ++it) {
            keys.push_back(it.key());
        }
        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys) {
            out[key] = canonicalizeJson(value.at(key));
        }
        return out;
    }
    if (value.is_array()) {
        ordered_json out = ordered_json::array();
        for (const auto& entry : value) {
            out.push_back(canonicalizeJson(entry));
        }
        return out;
    }
    return value;
}

std::string toHexLower(const uint8_t* data, std::size_t len) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        out[2 * i] = kHex[(data[i] >> 4) & 0x0F];
        out[2 * i + 1] = kHex[data[i] & 0x0F];
    }
    return out;
}

bool sha256File(const std::filesystem::path& path, std::string& sha_out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    SHA256_CTX ctx;
    if (SHA256_Init(&ctx) != 1) {
        return false;
    }

    std::array<uint8_t, 16384> buffer{};
    while (in.good()) {
        in.read(reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
        const std::streamsize n = in.gcount();
        if (n > 0) {
            if (SHA256_Update(&ctx, buffer.data(), static_cast<std::size_t>(n)) != 1) {
                return false;
            }
        }
    }

    std::array<uint8_t, SHA256_DIGEST_LENGTH> digest{};
    if (SHA256_Final(digest.data(), &ctx) != 1) {
        return false;
    }
    sha_out = toHexLower(digest.data(), digest.size());
    return true;
}

bool decodeBase64Url(std::string input, std::vector<uint8_t>& out_bytes) {
    if (input.empty()) {
        out_bytes.clear();
        return false;
    }

    std::replace(input.begin(), input.end(), '-', '+');
    std::replace(input.begin(), input.end(), '_', '/');
    while ((input.size() % 4u) != 0u) {
        input.push_back('=');
    }

    std::vector<uint8_t> decoded((input.size() / 4u) * 3u);
    const int n = EVP_DecodeBlock(
        decoded.data(),
        reinterpret_cast<const unsigned char*>(input.data()),
        static_cast<int>(input.size()));
    if (n < 0) {
        out_bytes.clear();
        return false;
    }

    std::size_t out_len = static_cast<std::size_t>(n);
    if (!input.empty() && input[input.size() - 1] == '=') {
        --out_len;
    }
    if (input.size() >= 2 && input[input.size() - 2] == '=') {
        --out_len;
    }
    decoded.resize(out_len);
    out_bytes = std::move(decoded);
    return true;
}

bool isAllowedJwsAlg(const std::string& alg) {
    return alg == "EdDSA" || alg == "ES256" || alg == "RS256";
}

bool isTrustedSigner(const std::vector<std::string>& trusted_kids, const std::string& kid) {
    return std::find(trusted_kids.begin(), trusted_kids.end(), kid) != trusted_kids.end();
}

bool parseJwsEnvelope(const json& jws,
                      std::string& payload_b64,
                      std::string& signature_b64,
                      std::string& kid,
                      std::string& alg) {
    if (!jws.is_object()) {
        return false;
    }
    payload_b64 = jws.value("payload", "");
    if (payload_b64.empty()) {
        return false;
    }

    const json* signature_entry = nullptr;
    if (jws.contains("signatures") && jws["signatures"].is_array() && !jws["signatures"].empty()) {
        signature_entry = &jws["signatures"].at(0);
    } else {
        signature_entry = &jws;
    }

    if (!signature_entry->is_object()) {
        return false;
    }

    signature_b64 = signature_entry->value("signature", "");
    if (signature_b64.empty()) {
        return false;
    }

    if (signature_entry->contains("header") && (*signature_entry)["header"].is_object()) {
        const auto& header = (*signature_entry)["header"];
        kid = header.value("kid", "");
        alg = header.value("alg", "");
    } else {
        kid.clear();
        alg.clear();
    }

    return true;
}

#if defined(_WIN32)
void* openSharedModule(const std::filesystem::path& module_path) {
    return reinterpret_cast<void*>(LoadLibraryA(module_path.string().c_str()));
}

void* resolveSharedSymbol(void* handle, const char* symbol_name) {
    if (!handle) {
        return nullptr;
    }
    return reinterpret_cast<void*>(
        GetProcAddress(reinterpret_cast<HMODULE>(handle), symbol_name));
}

void closeSharedModule(void* handle) {
    if (handle) {
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
    }
}
#else
void* openSharedModule(const std::filesystem::path& module_path) {
    return dlopen(module_path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
}

void* resolveSharedSymbol(void* handle, const char* symbol_name) {
    if (!handle) {
        return nullptr;
    }
    return dlsym(handle, symbol_name);
}

void closeSharedModule(void* handle) {
    if (handle) {
        dlclose(handle);
    }
}
#endif

const char* sharedLibraryExtension() {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

std::string builtinModuleLeafName(const std::string& plugin_id) {
    const std::size_t dot = plugin_id.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= plugin_id.size()) {
        return plugin_id;
    }
    return plugin_id.substr(dot + 1);
}

uint64_t hostNowUnixMs() {
    const auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

sb_auth_rc_t hostSecureRandom(uint8_t* out, uint32_t len) {
    if (out == nullptr || len == 0) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    return RAND_bytes(out, static_cast<int>(len)) == 1 ? SB_AUTH_RC_OK : SB_AUTH_RC_ERROR;
}

int hostConstTimeEqual(const uint8_t* a, const uint8_t* b, uint32_t len) {
    if (a == nullptr || b == nullptr) {
        return 0;
    }
    return CRYPTO_memcmp(a, b, static_cast<std::size_t>(len)) == 0 ? 1 : 0;
}

sb_auth_rc_t hostResolveUserByName(sb_auth_slice_t /*username*/,
                                   uint8_t /*out_user_uuid*/[SB_AUTH_UUID_BYTES]) {
    return SB_AUTH_RC_UNSUPPORTED;
}

sb_auth_rc_t hostResolveUserByExternalSubject(
    sb_auth_slice_t /*issuer*/,
    sb_auth_slice_t /*subject*/,
    uint8_t /*out_user_uuid*/[SB_AUTH_UUID_BYTES]) {
    return SB_AUTH_RC_UNSUPPORTED;
}

sb_auth_rc_t hostEmitAuditEvent(sb_auth_slice_t /*event_name*/, sb_auth_slice_t /*event_json*/) {
    return SB_AUTH_RC_OK;
}

sb_auth_rc_t hostReadPolicyValue(sb_auth_slice_t /*key*/, sb_auth_slice_t* /*out_value*/) {
    return SB_AUTH_RC_UNSUPPORTED;
}

void* hostAlloc(uint32_t size) {
    if (size == 0) {
        return nullptr;
    }
    return std::malloc(static_cast<std::size_t>(size));
}

void hostDealloc(void* ptr) {
    std::free(ptr);
}

}  // namespace

const char* authPluginRejectReasonToString(AuthPluginRejectReason reason) {
    switch (reason) {
        case AuthPluginRejectReason::NONE:
            return "NONE";
        case AuthPluginRejectReason::AUTH_PLUGIN_MANIFEST_INVALID:
            return "AUTH_PLUGIN_MANIFEST_INVALID";
        case AuthPluginRejectReason::AUTH_PLUGIN_SIGNATURE_INVALID:
            return "AUTH_PLUGIN_SIGNATURE_INVALID";
        case AuthPluginRejectReason::AUTH_PLUGIN_SIGNER_UNTRUSTED:
            return "AUTH_PLUGIN_SIGNER_UNTRUSTED";
        case AuthPluginRejectReason::AUTH_PLUGIN_POLICY_DENIED:
            return "AUTH_PLUGIN_POLICY_DENIED";
        case AuthPluginRejectReason::AUTH_PLUGIN_ID_UNKNOWN:
            return "AUTH_PLUGIN_ID_UNKNOWN";
        case AuthPluginRejectReason::AUTH_PLUGIN_DIGEST_MISMATCH:
            return "AUTH_PLUGIN_DIGEST_MISMATCH";
        case AuthPluginRejectReason::AUTH_PLUGIN_ABI_INCOMPATIBLE:
            return "AUTH_PLUGIN_ABI_INCOMPATIBLE";
        case AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED:
            return "AUTH_PLUGIN_LOAD_FAILED";
        default:
            return "UNKNOWN";
    }
}

AuthPluginManagerConfig AuthPluginManagerConfig::defaults() {
    AuthPluginManagerConfig config;
    config.truststore_path = kDefaultTrustStorePath;
    config.policy_path = kDefaultPolicyPath;
    config.plugin_root = "";
    config.fail_on_unlisted_plugins = true;
    return config;
}

AuthPluginManager::AuthPluginManager() = default;

AuthPluginManager::~AuthPluginManager() {
    shutdown();
}

const char* AuthPluginManager::defaultTrustStorePath() {
    return kDefaultTrustStorePath;
}

const char* AuthPluginManager::defaultPolicyPath() {
    return kDefaultPolicyPath;
}

core::Status AuthPluginManager::initialize(const AuthPluginManagerConfig& config,
                                           core::ErrorContext* ctx) {
    shutdown();
    config_ = config;
    if (config_.truststore_path.empty()) {
        config_.truststore_path = kDefaultTrustStorePath;
    }
    if (config_.policy_path.empty()) {
        config_.policy_path = kDefaultPolicyPath;
    }

    auto status = initializeHostApi();
    if (status != core::Status::OK) {
        if (ctx && ctx->message.empty()) {
            ctx->message = "Failed to initialize host API for AuthPluginManager";
        }
        return status;
    }

    status = loadTrustStore(ctx);
    if (status != core::Status::OK) {
        return status;
    }

    status = loadPolicy(ctx);
    if (status != core::Status::OK) {
        return status;
    }

    status = admitPlugins(ctx);
    if (status != core::Status::OK) {
        return status;
    }

    initialized_ = true;
    return core::Status::OK;
}

core::Status AuthPluginManager::reload(core::ErrorContext* ctx) {
    if (config_.truststore_path.empty() || config_.policy_path.empty()) {
        if (ctx) {
            ctx->message = "AuthPluginManager reload requested before initialize";
        }
        return core::Status::INVALID_ARGUMENT;
    }
    initialized_ = false;
    auto status = loadTrustStore(ctx);
    if (status != core::Status::OK) {
        return status;
    }
    status = loadPolicy(ctx);
    if (status != core::Status::OK) {
        return status;
    }
    status = admitPlugins(ctx);
    if (status != core::Status::OK) {
        return status;
    }
    initialized_ = true;
    return core::Status::OK;
}

void AuthPluginManager::shutdown() {
    initialized_ = false;
    closeRuntimePlugins();
    resetState();
}

bool AuthPluginManager::hasPlugin(const std::string& plugin_id) const {
    return plugin_index_.find(plugin_id) != plugin_index_.end();
}

bool AuthPluginManager::isMethodAvailable(const std::string& method_id) const {
    return method_to_plugin_.find(method_id) != method_to_plugin_.end();
}

bool AuthPluginManager::isRuntimeMethodAvailable(const std::string& method_id) const {
    std::lock_guard<std::mutex> lock(runtime_mutex_);
    return runtime_method_index_.find(method_id) != runtime_method_index_.end();
}

core::Status AuthPluginManager::beginAuth(const std::string& method_id,
                                          const sb_auth_connection_ctx_v1& conn_ctx,
                                          const std::vector<uint8_t>& client_payload,
                                          sb_auth_exchange_t* inout_exchange,
                                          sb_auth_step_result_v1* out_result,
                                          core::ErrorContext* ctx) {
    if (inout_exchange == nullptr || out_result == nullptr) {
        if (ctx) {
            ctx->message = "Auth plugin beginAuth requires exchange/result buffers";
        }
        return core::Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(runtime_mutex_);
    const auto runtime_it = runtime_method_index_.find(method_id);
    if (runtime_it == runtime_method_index_.end()) {
        if (ctx) {
            ctx->message = "Auth plugin runtime method unavailable: " + method_id;
        }
        return core::Status::NOT_FOUND;
    }
    if (runtime_it->second >= runtime_plugins_.size()) {
        if (ctx) {
            ctx->message = "Auth plugin runtime index out of range";
        }
        return core::Status::INTERNAL_ERROR;
    }

    RuntimePlugin& runtime = runtime_plugins_[runtime_it->second];
    if (!runtime.api || !runtime.api->begin_auth || runtime.instance == 0) {
        if (ctx) {
            ctx->message = "Auth plugin runtime begin_auth unavailable";
        }
        return core::Status::NOT_SUPPORTED;
    }

    std::memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = sizeof(*out_result);
    out_result->principal.struct_size = sizeof(out_result->principal);

    sb_auth_slice_t method_slice{
        reinterpret_cast<const uint8_t*>(method_id.data()),
        static_cast<uint32_t>(method_id.size())
    };
    sb_auth_slice_t payload_slice{
        client_payload.empty() ? nullptr : client_payload.data(),
        static_cast<uint32_t>(client_payload.size())
    };

    sb_auth_exchange_t exchange = *inout_exchange;
    const sb_auth_rc_t rc = runtime.api->begin_auth(runtime.instance,
                                                    method_slice,
                                                    &conn_ctx,
                                                    payload_slice,
                                                    &exchange,
                                                    out_result);
    out_result->rc = rc;
    *inout_exchange = exchange;
    return core::Status::OK;
}

core::Status AuthPluginManager::continueAuth(const std::string& method_id,
                                             sb_auth_exchange_t exchange,
                                             const std::vector<uint8_t>& client_payload,
                                             sb_auth_step_result_v1* out_result,
                                             core::ErrorContext* ctx) {
    if (out_result == nullptr) {
        if (ctx) {
            ctx->message = "Auth plugin continueAuth requires result buffer";
        }
        return core::Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(runtime_mutex_);
    const auto runtime_it = runtime_method_index_.find(method_id);
    if (runtime_it == runtime_method_index_.end()) {
        if (ctx) {
            ctx->message = "Auth plugin runtime method unavailable: " + method_id;
        }
        return core::Status::NOT_FOUND;
    }
    if (runtime_it->second >= runtime_plugins_.size()) {
        if (ctx) {
            ctx->message = "Auth plugin runtime index out of range";
        }
        return core::Status::INTERNAL_ERROR;
    }

    RuntimePlugin& runtime = runtime_plugins_[runtime_it->second];
    if (!runtime.api || !runtime.api->continue_auth || runtime.instance == 0) {
        if (ctx) {
            ctx->message = "Auth plugin runtime continue_auth unavailable";
        }
        return core::Status::NOT_SUPPORTED;
    }

    std::memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = sizeof(*out_result);
    out_result->principal.struct_size = sizeof(out_result->principal);

    sb_auth_slice_t payload_slice{
        client_payload.empty() ? nullptr : client_payload.data(),
        static_cast<uint32_t>(client_payload.size())
    };

    const sb_auth_rc_t rc = runtime.api->continue_auth(runtime.instance,
                                                       exchange,
                                                       payload_slice,
                                                       out_result);
    out_result->rc = rc;
    return core::Status::OK;
}

void AuthPluginManager::abortAuth(const std::string& method_id, sb_auth_exchange_t exchange) {
    std::lock_guard<std::mutex> lock(runtime_mutex_);
    const auto runtime_it = runtime_method_index_.find(method_id);
    if (runtime_it == runtime_method_index_.end()) {
        return;
    }
    if (runtime_it->second >= runtime_plugins_.size()) {
        return;
    }
    RuntimePlugin& runtime = runtime_plugins_[runtime_it->second];
    if (!runtime.api || !runtime.api->abort_auth || runtime.instance == 0) {
        return;
    }
    runtime.api->abort_auth(runtime.instance, exchange);
}

bool AuthPluginManager::resolveMethodIdForAuthType(AuthType auth_type,
                                                   std::string& method_id_out) const {
    method_id_out = mapAuthTypeToMethodId(auth_type);
    return !method_id_out.empty();
}

bool AuthPluginManager::isAuthTypeAvailable(AuthType auth_type) const {
    std::string method_id;
    if (!resolveMethodIdForAuthType(auth_type, method_id)) {
        return false;
    }
    return isMethodAvailable(method_id);
}

core::Status AuthPluginManager::initializeHostApi() {
    host_api_ = {};
    host_api_.struct_size = sizeof(sb_auth_host_api_v1);
    host_api_.now_unix_ms = &hostNowUnixMs;
    host_api_.secure_random = &hostSecureRandom;
    host_api_.const_time_equal = &hostConstTimeEqual;
    host_api_.resolve_user_by_name = &hostResolveUserByName;
    host_api_.resolve_user_by_external_subject = &hostResolveUserByExternalSubject;
    host_api_.emit_audit_event = &hostEmitAuditEvent;
    host_api_.read_policy_value = &hostReadPolicyValue;
    host_api_.alloc = &hostAlloc;
    host_api_.dealloc = &hostDealloc;
    return core::Status::OK;
}

core::Status AuthPluginManager::loadTrustStore(core::ErrorContext* ctx) {
    trusted_signer_kids_.clear();

    const std::filesystem::path truststore_path(config_.truststore_path);
    if (!std::filesystem::exists(truststore_path)) {
        // Local development fallback: keep deterministic signer set when trust store is absent.
        trusted_signer_kids_ = {
            "sb-release-kid-2026",
            "sb-security-kid-2026",
            "sb-enterprise-kid-2026"
        };
        return core::Status::OK;
    }

    std::string truststore_text;
    if (!readTextFile(truststore_path, truststore_text)) {
        if (ctx) {
            ctx->message = "Unable to read auth plugin trust store: " + truststore_path.string();
        }
        return core::Status::IO_ERROR;
    }

    json truststore_json;
    try {
        truststore_json = json::parse(truststore_text);
    } catch (const std::exception& ex) {
        if (ctx) {
            ctx->message = std::string("Invalid trust store JSON: ") + ex.what();
        }
        return core::Status::INVALID_ARGUMENT;
    }

    if (!truststore_json.is_object() || !truststore_json.contains("keys") ||
        !truststore_json["keys"].is_array()) {
        if (ctx) {
            ctx->message = "Trust store must contain a 'keys' array";
        }
        return core::Status::INVALID_ARGUMENT;
    }

    for (const auto& key : truststore_json["keys"]) {
        if (!key.is_object()) {
            continue;
        }
        const std::string kid = trimAscii(key.value("kid", ""));
        const std::string use = trimAscii(key.value("use", ""));
        const bool active = key.value("active", true);
        if (kid.empty() || use != "sig" || !active) {
            continue;
        }
        trusted_signer_kids_.push_back(kid);
    }

    if (trusted_signer_kids_.empty()) {
        if (ctx) {
            ctx->message = "Trust store has no active signing keys";
        }
        return core::Status::INVALID_ARGUMENT;
    }
    return core::Status::OK;
}

core::Status AuthPluginManager::loadPolicy(core::ErrorContext* ctx) {
    allowed_plugins_.clear();

    const std::filesystem::path policy_path(config_.policy_path);
    if (!std::filesystem::exists(policy_path)) {
        for (const auto& builtin : kBuiltinPhase1Plugins) {
            PolicyRule rule;
            rule.required = true;
            rule.allowed_signers = {"sb-release-kid-2026", "sb-security-kid-2026"};
            for (std::size_t i = 0; i < builtin.method_count; ++i) {
                rule.allowed_method_ids.push_back(builtin.methods[i]);
            }
            allowed_plugins_.emplace(builtin.plugin_id, std::move(rule));
        }
        config_.fail_on_unlisted_plugins = true;
        return core::Status::OK;
    }

    std::string policy_text;
    if (!readTextFile(policy_path, policy_text)) {
        if (ctx) {
            ctx->message = "Unable to read auth plugin policy: " + policy_path.string();
        }
        return core::Status::IO_ERROR;
    }

    json policy_json;
    try {
        policy_json = json::parse(policy_text);
    } catch (const std::exception& ex) {
        if (ctx) {
            ctx->message = std::string("Invalid policy JSON: ") + ex.what();
        }
        return core::Status::INVALID_ARGUMENT;
    }

    if (!policy_json.is_object() || !policy_json.contains("allowed_plugins") ||
        !policy_json["allowed_plugins"].is_object()) {
        if (ctx) {
            ctx->message = "Policy must contain object 'allowed_plugins'";
        }
        return core::Status::INVALID_ARGUMENT;
    }

    config_.fail_on_unlisted_plugins =
        policy_json.value("fail_on_unlisted_plugins", config_.fail_on_unlisted_plugins);

    for (auto it = policy_json["allowed_plugins"].begin();
         it != policy_json["allowed_plugins"].end(); ++it) {
        if (!it.value().is_object()) {
            continue;
        }

        PolicyRule rule;
        rule.required = it.value().value("required", false);

        if (it.value().contains("allowed_signers") && it.value()["allowed_signers"].is_array()) {
            for (const auto& signer : it.value()["allowed_signers"]) {
                if (!signer.is_string()) {
                    continue;
                }
                rule.allowed_signers.push_back(trimAscii(signer.get<std::string>()));
            }
        }

        if (it.value().contains("allowed_method_ids") && it.value()["allowed_method_ids"].is_array()) {
            for (const auto& method : it.value()["allowed_method_ids"]) {
                if (!method.is_string()) {
                    continue;
                }
                rule.allowed_method_ids.push_back(trimAscii(method.get<std::string>()));
            }
        }

        allowed_plugins_.emplace(it.key(), std::move(rule));
    }

    if (allowed_plugins_.empty()) {
        if (ctx) {
            ctx->message = "Policy did not produce any allowed plugins";
        }
        return core::Status::INVALID_ARGUMENT;
    }

    return core::Status::OK;
}

core::Status AuthPluginManager::admitPlugins(core::ErrorContext* ctx) {
    closeRuntimePlugins();
    {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        runtime_method_index_.clear();
    }
    loaded_plugins_.clear();
    admission_issues_.clear();
    method_to_plugin_.clear();
    plugin_index_.clear();
    missing_required_plugins_.clear();

    registerBuiltinPhase1Plugins();

    for (const auto& entry : allowed_plugins_) {
        const std::string& plugin_id = entry.first;
        const PolicyRule& rule = entry.second;
        if (hasPlugin(plugin_id)) {
            continue;
        }
        auto status = admitExternalPlugin(plugin_id, rule, ctx);
        if (status != core::Status::OK) {
            continue;
        }
    }

    for (const auto& entry : allowed_plugins_) {
        const std::string& plugin_id = entry.first;
        const PolicyRule& rule = entry.second;
        if (rule.required && !hasPlugin(plugin_id)) {
            missing_required_plugins_.push_back(plugin_id);
        }
    }

    if (!missing_required_plugins_.empty()) {
        if (ctx) {
            std::ostringstream oss;
            oss << "AuthPluginManager required plugin set incomplete: ";
            for (std::size_t i = 0; i < missing_required_plugins_.size(); ++i) {
                if (i > 0) {
                    oss << ", ";
                }
                oss << missing_required_plugins_[i];
            }
            ctx->message = oss.str();
        }
        return core::Status::NOT_FOUND;
    }
    return core::Status::OK;
}

void AuthPluginManager::registerBuiltinPhase1Plugins() {
    auto register_runtime_methods = [&](const std::vector<AuthPluginMethodInfo>& methods,
                                        std::size_t runtime_index) {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        for (const auto& method : methods) {
            runtime_method_index_[method.method_id] = runtime_index;
        }
    };

    auto try_attach_builtin_runtime = [&](const AuthPluginInfo& info) {
        if (config_.plugin_root.empty()) {
            return;
        }

        const std::string leaf = builtinModuleLeafName(info.plugin_id);
        const std::filesystem::path module_path =
            std::filesystem::path(config_.plugin_root) /
            info.plugin_id /
            ("libscratchbird_auth_" + leaf + sharedLibraryExtension());
        if (!std::filesystem::exists(module_path)) {
            return;
        }

        void* module_handle = openSharedModule(module_path);
        if (!module_handle) {
            recordAdmissionIssue(
                info.plugin_id,
                AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED,
                "Failed to load built-in auth plugin module");
            return;
        }

        auto get_api = reinterpret_cast<sb_auth_plugin_get_api_v1_fn>(
            resolveSharedSymbol(module_handle, kPluginSymbolGetApi));
        if (!get_api) {
            closeSharedModule(module_handle);
            recordAdmissionIssue(
                info.plugin_id,
                AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED,
                "Built-in plugin missing symbol sb_auth_plugin_get_api_v1");
            return;
        }

        const sb_auth_plugin_descriptor_v1* descriptor = nullptr;
        const sb_auth_plugin_api_v1* api = nullptr;
        const sb_auth_rc_t rc = get_api(
            SB_AUTH_ABI_MAJOR,
            &host_api_,
            &descriptor,
            &api);
        if (rc != SB_AUTH_RC_OK || descriptor == nullptr || api == nullptr) {
            closeSharedModule(module_handle);
            recordAdmissionIssue(
                info.plugin_id,
                AuthPluginRejectReason::AUTH_PLUGIN_ABI_INCOMPATIBLE,
                "Built-in plugin ABI handshake rejected");
            return;
        }
        if (descriptor->abi_major != SB_AUTH_ABI_MAJOR ||
            descriptor->abi_minor > SB_AUTH_ABI_MINOR) {
            closeSharedModule(module_handle);
            recordAdmissionIssue(
                info.plugin_id,
                AuthPluginRejectReason::AUTH_PLUGIN_ABI_INCOMPATIBLE,
                "Built-in plugin ABI version mismatch");
            return;
        }
        if (std::string(descriptor->plugin_id) != info.plugin_id) {
            closeSharedModule(module_handle);
            recordAdmissionIssue(
                info.plugin_id,
                AuthPluginRejectReason::AUTH_PLUGIN_ID_UNKNOWN,
                "Built-in plugin descriptor id mismatch");
            return;
        }

        RuntimePlugin runtime_plugin;
        runtime_plugin.info = info;
        runtime_plugin.module_path = module_path.string();
        runtime_plugin.descriptor = descriptor;
        runtime_plugin.api = api;
        runtime_plugin.module_handle = module_handle;

        if (runtime_plugin.api->create_instance) {
            sb_auth_plugin_instance_t instance = 0;
            const sb_auth_rc_t create_rc =
                runtime_plugin.api->create_instance(&instance);
            if (create_rc != SB_AUTH_RC_OK || instance == 0) {
                closeSharedModule(module_handle);
                recordAdmissionIssue(
                    info.plugin_id,
                    AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED,
                    "Built-in plugin create_instance failed");
                return;
            }
            runtime_plugin.instance = instance;
        }

        if (runtime_plugin.api->configure_instance) {
            const sb_auth_slice_t empty_config{nullptr, 0};
            const sb_auth_rc_t configure_rc =
                runtime_plugin.api->configure_instance(runtime_plugin.instance, empty_config);
            if (configure_rc != SB_AUTH_RC_OK &&
                configure_rc != SB_AUTH_RC_UNSUPPORTED) {
                if (runtime_plugin.api->destroy_instance && runtime_plugin.instance != 0) {
                    runtime_plugin.api->destroy_instance(runtime_plugin.instance);
                    runtime_plugin.instance = 0;
                }
                closeSharedModule(module_handle);
                recordAdmissionIssue(
                    info.plugin_id,
                    AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED,
                    "Built-in plugin configure_instance failed");
                return;
            }
        }

        const std::size_t runtime_index = runtime_plugins_.size();
        runtime_plugins_.push_back(std::move(runtime_plugin));
        register_runtime_methods(info.methods, runtime_index);
    };

    for (const auto& builtin : kBuiltinPhase1Plugins) {
        auto policy_it = allowed_plugins_.find(builtin.plugin_id);
        if (policy_it == allowed_plugins_.end()) {
            if (config_.fail_on_unlisted_plugins) {
                recordAdmissionIssue(
                    builtin.plugin_id,
                    AuthPluginRejectReason::AUTH_PLUGIN_ID_UNKNOWN,
                    "Built-in plugin not present in policy allowlist");
            }
            continue;
        }

        const PolicyRule& policy_rule = policy_it->second;

        AuthPluginInfo info;
        info.plugin_id = builtin.plugin_id;
        info.plugin_version = "builtin-1.0.0";
        info.abi_major = SB_AUTH_ABI_MAJOR;
        info.abi_minor = SB_AUTH_ABI_MINOR;
        info.required = policy_rule.required;
        info.builtin = true;

        for (std::size_t i = 0; i < builtin.method_count; ++i) {
            AuthPluginMethodInfo method;
            method.method_id = builtin.methods[i];
            method.method_flags = 0;
            method.legacy_wire_code = 0xFFFFFFFFu;
            info.methods.push_back(std::move(method));
        }

        if (!verifyMethodsAllowed(policy_rule, info.methods)) {
            recordAdmissionIssue(
                info.plugin_id,
                AuthPluginRejectReason::AUTH_PLUGIN_POLICY_DENIED,
                "Built-in plugin methods denied by policy");
            continue;
        }

        plugin_index_[info.plugin_id] = loaded_plugins_.size();
        loaded_plugins_.push_back(info);
        for (const auto& method : info.methods) {
            method_to_plugin_[method.method_id] = info.plugin_id;
        }
        try_attach_builtin_runtime(info);
    }
}

core::Status AuthPluginManager::admitExternalPlugin(const std::string& plugin_id,
                                                    const PolicyRule& policy_rule,
                                                    core::ErrorContext* ctx) {
    if (config_.plugin_root.empty()) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED,
            "Plugin root not configured");
        return core::Status::NOT_FOUND;
    }

    const std::filesystem::path plugin_dir = std::filesystem::path(config_.plugin_root) / plugin_id;
    const std::filesystem::path manifest_path = plugin_dir / "manifest.json";
    const std::filesystem::path jws_path = plugin_dir / "manifest.jws";
    if (!std::filesystem::exists(manifest_path) || !std::filesystem::exists(jws_path)) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_MANIFEST_INVALID,
            "Missing manifest.json or manifest.jws");
        return core::Status::NOT_FOUND;
    }

    std::string manifest_text;
    std::string jws_text;
    if (!readTextFile(manifest_path, manifest_text) || !readTextFile(jws_path, jws_text)) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_MANIFEST_INVALID,
            "Unable to read plugin manifest or signature file");
        return core::Status::IO_ERROR;
    }

    json manifest_json;
    json jws_json;
    try {
        manifest_json = json::parse(manifest_text);
        jws_json = json::parse(jws_text);
    } catch (const std::exception& ex) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_MANIFEST_INVALID,
            std::string("JSON parse failure: ") + ex.what());
        return core::Status::INVALID_ARGUMENT;
    }

    const std::string manifest_plugin_id = trimAscii(manifest_json.value("plugin_id", ""));
    if (manifest_plugin_id.empty() || manifest_plugin_id != plugin_id) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_ID_UNKNOWN,
            "Manifest plugin_id mismatch");
        return core::Status::INVALID_ARGUMENT;
    }

    const std::string canonical_manifest = canonicalizeJson(manifest_json).dump();

    std::string payload_b64;
    std::string signature_b64;
    std::string kid;
    std::string alg;
    if (!parseJwsEnvelope(jws_json, payload_b64, signature_b64, kid, alg)) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_SIGNATURE_INVALID,
            "Malformed manifest.jws envelope");
        return core::Status::INVALID_ARGUMENT;
    }

    std::vector<uint8_t> payload_bytes;
    if (!decodeBase64Url(payload_b64, payload_bytes)) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_SIGNATURE_INVALID,
            "Unable to decode manifest.jws payload");
        return core::Status::INVALID_ARGUMENT;
    }
    const std::string payload_json(payload_bytes.begin(), payload_bytes.end());
    if (payload_json != canonical_manifest) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_SIGNATURE_INVALID,
            "JWS payload does not match canonical manifest bytes");
        return core::Status::INVALID_ARGUMENT;
    }

    if (kid.empty() || !isAllowedJwsAlg(alg)) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_SIGNATURE_INVALID,
            "JWS header missing allowed signer kid/alg");
        return core::Status::INVALID_ARGUMENT;
    }

    std::vector<uint8_t> signature_bytes;
    if (!decodeBase64Url(signature_b64, signature_bytes) || signature_bytes.empty()) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_SIGNATURE_INVALID,
            "JWS signature field invalid");
        return core::Status::INVALID_ARGUMENT;
    }

    if (!isTrustedSigner(trusted_signer_kids_, kid)) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_SIGNER_UNTRUSTED,
            "Signer kid is not trusted");
        return core::Status::PERMISSION_DENIED;
    }

    if (!verifySignerAllowed(policy_rule, kid)) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_POLICY_DENIED,
            "Signer not allowed by plugin policy");
        return core::Status::PERMISSION_DENIED;
    }

    std::vector<AuthPluginMethodInfo> manifest_methods;
    if (!manifest_json.contains("supported_method_ids") || !manifest_json["supported_method_ids"].is_array()) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_MANIFEST_INVALID,
            "Manifest missing supported_method_ids array");
        return core::Status::INVALID_ARGUMENT;
    }
    for (const auto& method_id : manifest_json["supported_method_ids"]) {
        if (!method_id.is_string()) {
            continue;
        }
        AuthPluginMethodInfo method;
        method.method_id = trimAscii(method_id.get<std::string>());
        manifest_methods.push_back(std::move(method));
    }
    if (manifest_methods.empty()) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_MANIFEST_INVALID,
            "Manifest has no supported methods");
        return core::Status::INVALID_ARGUMENT;
    }

    if (!verifyMethodsAllowed(policy_rule, manifest_methods)) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_POLICY_DENIED,
            "Manifest method set denied by policy");
        return core::Status::PERMISSION_DENIED;
    }

    const std::string module_rel_path = trimAscii(manifest_json.value("module_path", ""));
    const std::string module_sha256 = toLowerAscii(trimAscii(manifest_json.value("module_sha256", "")));
    if (module_rel_path.empty() || module_sha256.empty()) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_MANIFEST_INVALID,
            "Manifest missing module_path or module_sha256");
        return core::Status::INVALID_ARGUMENT;
    }

    const std::filesystem::path module_path = plugin_dir / module_rel_path;
    std::string actual_sha256;
    if (!sha256File(module_path, actual_sha256)) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED,
            "Unable to read module for digest verification");
        return core::Status::NOT_FOUND;
    }
    if (actual_sha256 != module_sha256) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_DIGEST_MISMATCH,
            "Module digest mismatch");
        return core::Status::CHECKSUM_MISMATCH;
    }

    void* module_handle = openSharedModule(module_path);
    if (!module_handle) {
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED,
            "Failed to load module shared object");
        return core::Status::NOT_FOUND;
    }

    auto get_api = reinterpret_cast<sb_auth_plugin_get_api_v1_fn>(
        resolveSharedSymbol(module_handle, kPluginSymbolGetApi));
    if (!get_api) {
        closeSharedModule(module_handle);
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED,
            "Missing symbol sb_auth_plugin_get_api_v1");
        return core::Status::NOT_FOUND;
    }

    const sb_auth_plugin_descriptor_v1* descriptor = nullptr;
    const sb_auth_plugin_api_v1* api = nullptr;
    const sb_auth_rc_t rc = get_api(
        SB_AUTH_ABI_MAJOR,
        &host_api_,
        &descriptor,
        &api);
    if (rc != SB_AUTH_RC_OK || descriptor == nullptr || api == nullptr) {
        closeSharedModule(module_handle);
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_ABI_INCOMPATIBLE,
            "Plugin ABI handshake rejected");
        return core::Status::NOT_SUPPORTED;
    }
    if (descriptor->abi_major != SB_AUTH_ABI_MAJOR || descriptor->abi_minor > SB_AUTH_ABI_MINOR) {
        closeSharedModule(module_handle);
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_ABI_INCOMPATIBLE,
            "Plugin ABI version mismatch");
        return core::Status::NOT_SUPPORTED;
    }
    if (std::string(descriptor->plugin_id) != plugin_id) {
        closeSharedModule(module_handle);
        recordAdmissionIssue(
            plugin_id,
            AuthPluginRejectReason::AUTH_PLUGIN_ID_UNKNOWN,
            "Plugin descriptor id mismatch");
        return core::Status::INVALID_ARGUMENT;
    }

    AuthPluginInfo info;
    info.plugin_id = plugin_id;
    info.plugin_version = manifest_json.value("plugin_version", "0.0.0");
    info.abi_major = descriptor->abi_major;
    info.abi_minor = descriptor->abi_minor;
    info.required = policy_rule.required;
    info.builtin = false;

    for (const auto& method : manifest_methods) {
        info.methods.push_back(method);
    }

    RuntimePlugin runtime_plugin;
    runtime_plugin.info = info;
    runtime_plugin.module_path = module_path.string();
    runtime_plugin.descriptor = descriptor;
    runtime_plugin.api = api;
    runtime_plugin.module_handle = module_handle;

    if (runtime_plugin.api->create_instance) {
        sb_auth_plugin_instance_t instance = 0;
        const sb_auth_rc_t create_rc =
            runtime_plugin.api->create_instance(&instance);
        if (create_rc != SB_AUTH_RC_OK || instance == 0) {
            closeSharedModule(module_handle);
            recordAdmissionIssue(
                plugin_id,
                AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED,
                "Plugin create_instance failed");
            return core::Status::NOT_FOUND;
        }
        runtime_plugin.instance = instance;
    }

    if (runtime_plugin.api->configure_instance) {
        const sb_auth_slice_t empty_config{nullptr, 0};
        const sb_auth_rc_t configure_rc =
            runtime_plugin.api->configure_instance(runtime_plugin.instance, empty_config);
        if (configure_rc != SB_AUTH_RC_OK &&
            configure_rc != SB_AUTH_RC_UNSUPPORTED) {
            if (runtime_plugin.api->destroy_instance && runtime_plugin.instance != 0) {
                runtime_plugin.api->destroy_instance(runtime_plugin.instance);
                runtime_plugin.instance = 0;
            }
            closeSharedModule(module_handle);
            recordAdmissionIssue(
                plugin_id,
                AuthPluginRejectReason::AUTH_PLUGIN_LOAD_FAILED,
                "Plugin configure_instance failed");
            return core::Status::NOT_FOUND;
        }
    }

    const std::size_t runtime_index = runtime_plugins_.size();
    runtime_plugins_.push_back(std::move(runtime_plugin));
    {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        for (const auto& method : info.methods) {
            runtime_method_index_[method.method_id] = runtime_index;
        }
    }

    plugin_index_[info.plugin_id] = loaded_plugins_.size();
    loaded_plugins_.push_back(info);
    for (const auto& method : info.methods) {
        method_to_plugin_[method.method_id] = info.plugin_id;
    }

    return core::Status::OK;
}

bool AuthPluginManager::verifySignerAllowed(const PolicyRule& policy_rule,
                                            const std::string& kid) const {
    if (policy_rule.allowed_signers.empty()) {
        return true;
    }
    return std::find(policy_rule.allowed_signers.begin(),
                     policy_rule.allowed_signers.end(),
                     kid) != policy_rule.allowed_signers.end();
}

bool AuthPluginManager::verifyMethodsAllowed(
    const PolicyRule& policy_rule,
    const std::vector<AuthPluginMethodInfo>& methods) const {
    if (policy_rule.allowed_method_ids.empty()) {
        return true;
    }

    std::set<std::string> allowed;
    for (const auto& method_id : policy_rule.allowed_method_ids) {
        allowed.insert(method_id);
    }

    for (const auto& method : methods) {
        if (allowed.find(method.method_id) == allowed.end()) {
            return false;
        }
    }
    return true;
}

std::string AuthPluginManager::mapAuthTypeToMethodId(AuthType auth_type) const {
    switch (auth_type) {
        case AuthType::TRUST:
            return "scratchbird.auth.trust";
        case AuthType::REJECT:
            return "scratchbird.auth.reject";
        case AuthType::PASSWORD:
            return "scratchbird.auth.password_compat";
        case AuthType::MD5:
            return "scratchbird.auth.md5_legacy";
        case AuthType::SCRAM_SHA_256:
            return "scratchbird.auth.scram_sha_256";
        case AuthType::SCRAM_SHA_512:
            return "scratchbird.auth.scram_sha_512";
        case AuthType::CERTIFICATE:
            return "scratchbird.auth.certificate_x509";
        case AuthType::LDAP:
            return "scratchbird.auth.ldap_bind";
        case AuthType::KERBEROS:
            return "scratchbird.auth.kerberos_gssapi";
        case AuthType::PEER:
            return "scratchbird.auth.peer_uid";
        case AuthType::IDENT:
            return "scratchbird.auth.ident_rfc1413";
        case AuthType::RADIUS:
            return "scratchbird.auth.radius_pap";
        case AuthType::PAM:
            return "scratchbird.auth.pam_conversation";
        case AuthType::TOKEN:
            return "scratchbird.auth.authkey_token";
        default:
            return "";
    }
}

void AuthPluginManager::recordAdmissionIssue(const std::string& plugin_id,
                                             AuthPluginRejectReason reason,
                                             const std::string& detail) {
    AuthPluginAdmissionIssue issue;
    issue.plugin_id = plugin_id;
    issue.reason = reason;
    issue.detail = detail;
    admission_issues_.push_back(std::move(issue));
}

void AuthPluginManager::resetState() {
    loaded_plugins_.clear();
    admission_issues_.clear();
    missing_required_plugins_.clear();
    allowed_plugins_.clear();
    method_to_plugin_.clear();
    runtime_method_index_.clear();
    plugin_index_.clear();
    trusted_signer_kids_.clear();
}

void AuthPluginManager::closeRuntimePlugins() {
    std::lock_guard<std::mutex> lock(runtime_mutex_);
    for (auto& runtime : runtime_plugins_) {
        if (runtime.api && runtime.instance != 0 && runtime.api->destroy_instance) {
            runtime.api->destroy_instance(runtime.instance);
            runtime.instance = 0;
        }
        if (runtime.module_handle) {
            closeSharedModule(runtime.module_handle);
            runtime.module_handle = nullptr;
        }
    }
    runtime_plugins_.clear();
    runtime_method_index_.clear();
}

}  // namespace security
}  // namespace scratchbird
