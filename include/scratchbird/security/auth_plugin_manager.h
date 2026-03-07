/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

/**
 * Authentication plugin admission and registry manager.
 *
 * Owns signed plugin discovery, admission policy, built-in method exposure,
 * and runtime lookup for plugin-backed authentication methods.
 */

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/security/auth_method.h"
#include "scratchbird/security/auth_plugin_abi_v1.h"

namespace scratchbird {
namespace security {

enum class AuthPluginRejectReason : uint8_t {
    NONE = 0,
    AUTH_PLUGIN_MANIFEST_INVALID,
    AUTH_PLUGIN_SIGNATURE_INVALID,
    AUTH_PLUGIN_SIGNER_UNTRUSTED,
    AUTH_PLUGIN_POLICY_DENIED,
    AUTH_PLUGIN_ID_UNKNOWN,
    AUTH_PLUGIN_DIGEST_MISMATCH,
    AUTH_PLUGIN_ABI_INCOMPATIBLE,
    AUTH_PLUGIN_LOAD_FAILED
};

const char* authPluginRejectReasonToString(AuthPluginRejectReason reason);

struct AuthPluginAdmissionIssue {
    std::string plugin_id;
    AuthPluginRejectReason reason = AuthPluginRejectReason::NONE;
    std::string detail;
};

struct AuthPluginMethodInfo {
    std::string method_id;
    uint32_t method_flags = 0;
    uint32_t legacy_wire_code = 0xFFFFFFFFu;
};

struct AuthPluginInfo {
    std::string plugin_id;
    std::string plugin_version;
    uint32_t abi_major = 0;
    uint32_t abi_minor = 0;
    bool required = false;
    bool builtin = false;
    std::vector<AuthPluginMethodInfo> methods;
};

struct AuthPluginManagerConfig {
    std::string truststore_path;
    std::string policy_path;
    std::string plugin_root;
    bool fail_on_unlisted_plugins = true;

    static AuthPluginManagerConfig defaults();
};

class AuthPluginManager {
public:
    AuthPluginManager();
    ~AuthPluginManager();

    AuthPluginManager(const AuthPluginManager&) = delete;
    AuthPluginManager& operator=(const AuthPluginManager&) = delete;

    core::Status initialize(const AuthPluginManagerConfig& config,
                            core::ErrorContext* ctx = nullptr);
    core::Status reload(core::ErrorContext* ctx = nullptr);
    void shutdown();

    bool hasPlugin(const std::string& plugin_id) const;
    bool isMethodAvailable(const std::string& method_id) const;
    bool isRuntimeMethodAvailable(const std::string& method_id) const;
    bool resolveMethodIdForAuthType(AuthType auth_type, std::string& method_id_out) const;
    bool isAuthTypeAvailable(AuthType auth_type) const;

    core::Status beginAuth(const std::string& method_id,
                           const sb_auth_connection_ctx_v1& conn_ctx,
                           const std::vector<uint8_t>& client_payload,
                           sb_auth_exchange_t* inout_exchange,
                           sb_auth_step_result_v1* out_result,
                           core::ErrorContext* ctx = nullptr);
    core::Status continueAuth(const std::string& method_id,
                              sb_auth_exchange_t exchange,
                              const std::vector<uint8_t>& client_payload,
                              sb_auth_step_result_v1* out_result,
                              core::ErrorContext* ctx = nullptr);
    void abortAuth(const std::string& method_id, sb_auth_exchange_t exchange);

    const std::vector<AuthPluginInfo>& loadedPlugins() const { return loaded_plugins_; }
    const std::vector<AuthPluginAdmissionIssue>& admissionIssues() const { return admission_issues_; }
    const std::vector<std::string>& missingRequiredPlugins() const { return missing_required_plugins_; }

    static const char* defaultTrustStorePath();
    static const char* defaultPolicyPath();

private:
    struct PolicyRule {
        bool required = false;
        std::vector<std::string> allowed_signers;
        std::vector<std::string> allowed_method_ids;
    };

    struct RuntimePlugin {
        AuthPluginInfo info;
        std::string module_path;
        const sb_auth_plugin_descriptor_v1* descriptor = nullptr;
        const sb_auth_plugin_api_v1* api = nullptr;
        sb_auth_plugin_instance_t instance = 0;
        void* module_handle = nullptr;
    };

    core::Status initializeHostApi();
    core::Status loadTrustStore(core::ErrorContext* ctx);
    core::Status loadPolicy(core::ErrorContext* ctx);
    core::Status admitPlugins(core::ErrorContext* ctx);
    void registerBuiltinPhase1Plugins();
    core::Status admitExternalPlugin(const std::string& plugin_id,
                                     const PolicyRule& policy_rule,
                                     core::ErrorContext* ctx);
    bool verifySignerAllowed(const PolicyRule& policy_rule, const std::string& kid) const;
    bool verifyMethodsAllowed(const PolicyRule& policy_rule,
                              const std::vector<AuthPluginMethodInfo>& methods) const;
    std::string mapAuthTypeToMethodId(AuthType auth_type) const;
    void recordAdmissionIssue(const std::string& plugin_id,
                              AuthPluginRejectReason reason,
                              const std::string& detail);
    void resetState();
    void closeRuntimePlugins();

    AuthPluginManagerConfig config_;
    bool initialized_ = false;

    sb_auth_host_api_v1 host_api_{};
    std::vector<AuthPluginInfo> loaded_plugins_;
    std::vector<AuthPluginAdmissionIssue> admission_issues_;
    std::vector<std::string> missing_required_plugins_;

    std::unordered_map<std::string, PolicyRule> allowed_plugins_;
    std::unordered_map<std::string, std::string> method_to_plugin_;
    std::unordered_map<std::string, std::size_t> runtime_method_index_;
    std::unordered_map<std::string, std::size_t> plugin_index_;
    std::vector<std::string> trusted_signer_kids_;
    std::vector<RuntimePlugin> runtime_plugins_;
    mutable std::mutex runtime_mutex_;
};

}  // namespace security
}  // namespace scratchbird
