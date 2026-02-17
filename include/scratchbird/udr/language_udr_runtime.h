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

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::udr
{

    enum class LanguageUdrModuleStatus : uint8_t
    {
        ACTIVE = 0,
        DISABLED = 1,
        REVOKED = 2
    };

    enum class LanguageUdrSignatureStatus : uint8_t
    {
        TRUSTED = 0,
        UNTRUSTED = 1,
        UNKNOWN = 2
    };

    struct LanguageUdrRegistration
    {
        core::ID module_id;
        std::string module_name;
        std::string engine_profile_id;
        std::string engine_profile_version;
        std::string translation_mode;
        std::string module_semver;
        std::string artifact_hash;
        LanguageUdrSignatureStatus signature_status = LanguageUdrSignatureStatus::UNKNOWN;
        std::string capability_set_hash;
        LanguageUdrModuleStatus status = LanguageUdrModuleStatus::ACTIVE;
    };

    struct LanguageUdrCompileRequest
    {
        core::ID request_id;
        std::string profile_id;
        std::string profile_version;
        std::string payload_format;
        std::vector<uint8_t> payload;
        std::string session_option_signature;
        core::ID principal_id;
        std::string role_context_signature;
        uint64_t transaction_id = 0;
        uint64_t catalog_epoch = 0;
        uint64_t security_epoch = 0;
        std::string native_feature_key;
        bool compile_permission_granted = true;
    };

    class LanguageUdrRegistry
    {
    public:
        auto registerModule(const LanguageUdrRegistration &registration,
                            const std::vector<std::string> &enabled_feature_keys,
                            core::ErrorContext *ctx = nullptr) -> core::Status;

        auto setModuleStatus(const core::ID &module_id, LanguageUdrModuleStatus status,
                             core::ErrorContext *ctx = nullptr) -> core::Status;

        auto resolveActiveModule(const std::string &profile_id,
                                 const std::string &profile_version,
                                 LanguageUdrRegistration &resolved_out,
                                 core::ErrorContext *ctx = nullptr) const -> core::Status;

        auto ensureFeatureEnabled(const std::string &profile_id,
                                  const std::string &profile_version,
                                  const std::string &native_feature_key,
                                  core::ErrorContext *ctx = nullptr) const -> core::Status;

        static auto computeCapabilitySetHash(const std::vector<std::string> &enabled_feature_keys)
            -> std::string;

    private:
        struct ModuleEntry
        {
            LanguageUdrRegistration registration;
            std::unordered_set<std::string> enabled_features;
        };

        mutable std::mutex mutex_;
        std::unordered_map<core::ID, ModuleEntry, core::IDHash> modules_;
    };

    class LanguageUdrRuntimeBoundary
    {
    public:
        static auto validateCompileRequest(const LanguageUdrCompileRequest &request,
                                           core::ErrorContext *ctx = nullptr) -> core::Status;

        static auto preflightCompile(const LanguageUdrRegistry &registry,
                                     const LanguageUdrCompileRequest &request,
                                     LanguageUdrRegistration &selected_module_out,
                                     core::ErrorContext *ctx = nullptr) -> core::Status;
    };

} // namespace scratchbird::udr

